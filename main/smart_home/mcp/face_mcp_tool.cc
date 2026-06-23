#include "face_mcp_tool.h"

#include "mcp_server.h"
#include "board.h"
#include "esp_video.h"
#include "smart_home/services/face_recognition.h"
#include "jpg/image_to_jpeg.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <thread>
#include <cstring>
#include <string>
#include <vector>

static const char *TAG = "FaceMcp";

// 将 RGB565 帧编码为 JPEG
static std::string encode_jpeg(const uint8_t *data, size_t len, int width, int height,
                                uint32_t format, int quality = 80)
{
    // 创建 JPEG 队列
    QueueHandle_t jpeg_queue = xQueueCreate(40, sizeof(JpegChunk));
    if (!jpeg_queue) {
        ESP_LOGE(TAG, "Failed to create JPEG queue");
        return "";
    }

    // 编码线程(image_to_jpeg_cb 内部不修改源数据,但签名要求非 const,这里 const_cast)
    std::thread encoder([&]() {
        bool ok = image_to_jpeg_cb(
            const_cast<uint8_t *>(data), len, (uint16_t)width, (uint16_t)height, (v4l2_pix_fmt_t)format, quality,
            [](void *arg, size_t index, const void *chunk_data, size_t chunk_len) -> size_t {
                auto queue = static_cast<QueueHandle_t>(arg);
                JpegChunk chunk = {.data = nullptr, .len = chunk_len};
                if (index == 0 && chunk_data && chunk_len > 0) {
                    chunk.data = (uint8_t *)heap_caps_aligned_alloc(16, chunk_len,
                                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (chunk.data) {
                        memcpy(chunk.data, chunk_data, chunk_len);
                    } else {
                        chunk.len = 0;
                    }
                } else {
                    chunk.len = 0;
                }
                xQueueSend(queue, &chunk, portMAX_DELAY);
                return chunk_len;
            },
            jpeg_queue);
        if (!ok) {
            JpegChunk chunk = {.data = nullptr, .len = 0};
            xQueueSend(jpeg_queue, &chunk, portMAX_DELAY);
        }
    });

    // 收集 JPEG 数据
    std::string jpeg_data;
    while (true) {
        JpegChunk chunk;
        if (xQueueReceive(jpeg_queue, &chunk, pdMS_TO_TICKS(5000)) != pdPASS) {
            ESP_LOGE(TAG, "JPEG encode timeout");
            break;
        }
        if (chunk.data == nullptr && chunk.len == 0) {
            // 结束标记或错误
            break;
        }
        if (chunk.data && chunk.len > 0) {
            jpeg_data.append((char *)chunk.data, chunk.len);
            free(chunk.data);
        }
    }

    encoder.join();
    vQueueDelete(jpeg_queue);

    ESP_LOGI(TAG, "JPEG encoded: %d bytes", (int)jpeg_data.size());
    return jpeg_data;
}

// 采集一帧并返回 JPEG 数据
static std::string capture_jpeg()
{
    auto *camera = Board::GetInstance().GetCamera();
    if (!camera) {
        throw std::runtime_error("Camera not available");
    }

    auto *esp_video = dynamic_cast<EspVideo *>(camera);
    if (!esp_video) {
        throw std::runtime_error("Camera is not EspVideo");
    }

    EspVideo::CapturedFrame frame;
    if (!esp_video->CaptureFrame(frame)) {
        throw std::runtime_error("Failed to capture frame");
    }

    std::string jpeg = encode_jpeg(frame.data, frame.len, frame.width, frame.height, frame.format);
    free(frame.data);
    return jpeg;
}

extern "C" void FaceMcp_RegisterTools(void)
{
    auto &server = McpServer::GetInstance();

    // 1. 获取已注册人脸数量
    server.AddTool("self.face.list",
        "List registered faces count and info from the face database.",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            (void)properties;
            auto &face_recog = smart_home::FaceRecognition::GetInstance();
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "count", face_recog.GetFaceCount());
            return root;
        });

    // 2. 删除人脸(按 ID)
    server.AddTool("self.face.delete",
        "Delete a registered face by numeric id.",
        PropertyList({
            Property("id", kPropertyTypeInteger, 0, 65535),
        }),
        [](const PropertyList &properties) -> ReturnValue {
            uint16_t id = (uint16_t)properties["id"].value<int>();
            auto &face_recog = smart_home::FaceRecognition::GetInstance();
            if (face_recog.DeleteFace(id)) {
                return true;
            }
            return false;
        });

    // 3. 清空所有人脸
    server.AddTool("self.face.clear_all",
        "Clear all registered faces from the database.",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            (void)properties;
            auto &face_recog = smart_home::FaceRecognition::GetInstance();
            if (face_recog.ClearAllFaces()) {
                return true;
            }
            return false;
        });

    // 4. 识别人脸(采集一帧并识别)
    server.AddTool("self.face.recognize",
        "Capture a frame from the camera and recognize the face. Returns the user id and similarity if recognized, or 'unknown' if not.",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            (void)properties;
            auto &face_recog = smart_home::FaceRecognition::GetInstance();
            if (!face_recog.IsInitialized()) {
                if (!face_recog.Initialize()) {
                    return std::string("Face recognition not available");
                }
            }

            auto *camera = Board::GetInstance().GetCamera();
            if (!camera) {
                return std::string("Camera not available");
            }
            auto *esp_video = dynamic_cast<EspVideo *>(camera);
            if (!esp_video) {
                return std::string("Camera type mismatch");
            }

            EspVideo::CapturedFrame frame;
            if (!esp_video->CaptureFrame(frame)) {
                return std::string("Capture failed");
            }

            std::vector<smart_home::FaceDetectResult> results;
            bool detected = face_recog.DetectFaces(frame.data, frame.width, frame.height, results);

            if (!detected || results.empty()) {
                free(frame.data);
                return std::string("No face detected");
            }

            smart_home::FaceRecognizeResult recog_result;
            bool ok = face_recog.RecognizeFace(frame.data, frame.width, frame.height,
                                                results[0], recog_result);
            free(frame.data);

            if (ok && recog_result.success) {
                cJSON *root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "id", recog_result.id);
                cJSON_AddNumberToObject(root, "similarity", recog_result.similarity);
                return root;
            }
            return std::string("Face not recognized");
        });

    // 5. 采集图像(返回 JPEG 给 LLM 进行视觉识别)
    server.AddTool("self.face.capture",
        "Capture a frame from the camera and return it as a JPEG image for visual recognition.",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            (void)properties;
            std::string jpeg = capture_jpeg();
            if (jpeg.empty()) {
                return std::string("Failed to capture image");
            }
            auto *img = new ImageContent("image/jpeg", jpeg);
            return img;
        });

    // 6. 注册人脸(采集一帧,提取特征,保存)
    server.AddTool("self.face.register",
        "Capture a frame and register a new face. Returns the assigned numeric id.",
        PropertyList({
            Property("user_name", kPropertyTypeString),
        }),
        [](const PropertyList &properties) -> ReturnValue {
            std::string user_name = properties["user_name"].value<std::string>();

            auto &face_recog = smart_home::FaceRecognition::GetInstance();
            if (!face_recog.IsInitialized()) {
                if (!face_recog.Initialize()) {
                    return std::string("Face recognition not available");
                }
            }

            auto *camera = Board::GetInstance().GetCamera();
            if (!camera) {
                return std::string("Camera not available");
            }
            auto *esp_video = dynamic_cast<EspVideo *>(camera);
            if (!esp_video) {
                return std::string("Camera type mismatch");
            }

            EspVideo::CapturedFrame frame;
            if (!esp_video->CaptureFrame(frame)) {
                return std::string("Capture failed");
            }

            std::vector<smart_home::FaceDetectResult> results;
            bool detected = face_recog.DetectFaces(frame.data, frame.width, frame.height, results);

            if (!detected || results.empty()) {
                free(frame.data);
                return std::string("No face detected");
            }

            std::string user_id;
            bool ok = face_recog.RegisterFace(frame.data, frame.width, frame.height,
                                               results[0], user_name, user_id);
            free(frame.data);

            if (ok) {
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "user_id", user_id.c_str());
                cJSON_AddStringToObject(root, "user_name", user_name.c_str());
                cJSON_AddBoolToObject(root, "success", true);
                return root;
            }
            return std::string("Failed to register face");
        });

    ESP_LOGI(TAG, "Face MCP tools registered");
}
