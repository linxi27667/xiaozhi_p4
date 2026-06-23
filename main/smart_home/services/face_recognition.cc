#include "face_recognition.h"

#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "dl_image_define.hpp"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <sys/stat.h>

static const char *TAG = "FaceRecog";

// 人脸数据库存储在 TF 卡。P4 EV Board 初始化阶段已经挂载 /sdcard，
// 避免在 SRAM 很紧张时再挂载一个 FAT+WL 分区。
#define FACE_DB_PATH "/sdcard/face.db"

namespace smart_home {

static bool face_db_storage_ready()
{
    struct stat st = {};
    if (stat("/sdcard", &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "Face DB storage is not ready: /sdcard not mounted");
        return false;
    }
    return true;
}

FaceRecognition &FaceRecognition::GetInstance()
{
    static FaceRecognition instance;
    return instance;
}

FaceRecognition::~FaceRecognition()
{
    if (detector_) {
        delete static_cast<HumanFaceDetect *>(detector_);
        detector_ = nullptr;
    }
    if (recognizer_) {
        delete static_cast<HumanFaceRecognizer *>(recognizer_);
        recognizer_ = nullptr;
    }
}

bool FaceRecognition::Initialize()
{
    if (initialized_) {
        return true;
    }

    if (!face_db_storage_ready()) {
        return false;
    }

    // 2. 创建检测器(使用默认 MSRMNP_S8_V1 模型)
    //    lazy_load=false 表示立即加载模型到内存
    try {
        auto *detector = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);
        detector_ = detector;
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Failed to create HumanFaceDetect: %s", e.what());
        return false;
    }

    // 3. 创建识别器(包含特征提取模型和数据库)
    //    数据库路径指向 FAT 分区
    try {
        auto *recognizer = new HumanFaceRecognizer(FACE_DB_PATH,
                                                    HumanFaceFeat::MFN_S8_V1,
                                                    false);
        recognizer_ = recognizer;
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Failed to create HumanFaceRecognizer: %s", e.what());
        delete static_cast<HumanFaceDetect *>(detector_);
        detector_ = nullptr;
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Face recognition initialized (detector + recognizer)");
    return true;
}

// 将 RGB565 数据转换为 RGB888
// esp-dl 的 HumanFaceDetect 支持 RGB565,但为了兼容性和稳定性,统一转 RGB888
static uint8_t *rgb565_to_rgb888(const uint8_t *rgb565, int width, int height)
{
    size_t rgb888_size = (size_t)width * height * 3;
    uint8_t *rgb888 = (uint8_t *)heap_caps_aligned_alloc(16, rgb888_size,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rgb888) {
        return nullptr;
    }
    for (int i = 0; i < width * height; i++) {
        uint16_t pixel = rgb565[i * 2] | (rgb565[i * 2 + 1] << 8);
        rgb888[i * 3 + 0] = (pixel & 0xF800) >> 8; // R
        rgb888[i * 3 + 1] = (pixel & 0x07E0) >> 3; // G
        rgb888[i * 3 + 2] = (pixel & 0x001F) << 3; // B
    }
    return rgb888;
}

bool FaceRecognition::DetectFaces(const uint8_t *data, int width, int height,
                                   std::vector<FaceDetectResult> &results)
{
    if (!initialized_ || !detector_) {
        return false;
    }

    // 转换为 RGB888
    uint8_t *rgb888 = rgb565_to_rgb888(data, width, height);
    if (!rgb888) {
        ESP_LOGE(TAG, "RGB565->RGB888 conversion failed (OOM)");
        return false;
    }

    // 构建 img_t
    dl::image::img_t img = {};
    img.data = rgb888;
    img.width = (uint16_t)width;
    img.height = (uint16_t)height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    // 运行检测
    auto *detector = static_cast<HumanFaceDetect *>(detector_);
    std::list<dl::detect::result_t> &detect_res = detector->run(img);

    bool found = !detect_res.empty();
    for (const auto &r : detect_res) {
        FaceDetectResult fr;
        fr.x = r.box[0];
        fr.y = r.box[1];
        fr.width = r.box[2] - r.box[0];
        fr.height = r.box[3] - r.box[1];
        fr.score = r.score;
        results.push_back(fr);
    }

    heap_caps_free(rgb888);
    return found;
}

bool FaceRecognition::RecognizeFace(const uint8_t *data, int width, int height,
                                     const FaceDetectResult &detect,
                                     FaceRecognizeResult &result)
{
    if (!initialized_ || !recognizer_) {
        result.error_msg = "Not initialized";
        return false;
    }

    // 转换为 RGB888
    uint8_t *rgb888 = rgb565_to_rgb888(data, width, height);
    if (!rgb888) {
        result.error_msg = "RGB565->RGB888 conversion failed";
        return false;
    }

    dl::image::img_t img = {};
    img.data = rgb888;
    img.width = (uint16_t)width;
    img.height = (uint16_t)height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    // 先检测(识别器需要 detect_res 列表)
    auto *detector = static_cast<HumanFaceDetect *>(detector_);
    std::list<dl::detect::result_t> &detect_res = detector->run(img);

    if (detect_res.empty()) {
        result.error_msg = "No face detected";
        heap_caps_free(rgb888);
        return false;
    }

    // 识别
    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    std::vector<dl::recognition::result_t> recog_res = recognizer->recognize(img, detect_res);

    heap_caps_free(rgb888);

    if (recog_res.empty()) {
        result.error_msg = "No match in database";
        return false;
    }

    // 取相似度最高的结果
    result.id = recog_res[0].id;
    result.similarity = recog_res[0].similarity;
    result.success = true;
    return true;
}

bool FaceRecognition::RegisterFace(const uint8_t *data, int width, int height,
                                    const FaceDetectResult &detect,
                                    const std::string &user_name,
                                    std::string &user_id)
{
    if (!initialized_ || !recognizer_) {
        return false;
    }

    // 转换为 RGB888
    uint8_t *rgb888 = rgb565_to_rgb888(data, width, height);
    if (!rgb888) {
        return false;
    }

    dl::image::img_t img = {};
    img.data = rgb888;
    img.width = (uint16_t)width;
    img.height = (uint16_t)height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    // 检测
    auto *detector = static_cast<HumanFaceDetect *>(detector_);
    std::list<dl::detect::result_t> &detect_res = detector->run(img);

    if (detect_res.empty()) {
        heap_caps_free(rgb888);
        return false;
    }

    // 注册
    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    esp_err_t ret = recognizer->enroll(img, detect_res);

    heap_caps_free(rgb888);

    if (ret != ESP_OK) {
        return false;
    }

    // user_id 即为数据库中的 ID(数字)
    user_id = std::to_string(recognizer->get_num_feats());
    return true;
}

int FaceRecognition::GetFaceCount()
{
    if (!initialized_ || !recognizer_) {
        return 0;
    }
    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    return recognizer->get_num_feats();
}

bool FaceRecognition::ClearAllFaces()
{
    if (!initialized_ || !recognizer_) {
        return false;
    }
    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    return recognizer->clear_all_feats() == ESP_OK;
}

bool FaceRecognition::DeleteFace(uint16_t id)
{
    if (!initialized_ || !recognizer_) {
        return false;
    }
    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    return recognizer->delete_feat(id) == ESP_OK;
}

} // namespace smart_home
