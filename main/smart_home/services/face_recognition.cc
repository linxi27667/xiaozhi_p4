#include "face_recognition.h"

#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "dl_image_define.hpp"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

static const char *TAG = "FaceRecog";

// 人脸数据库存储在 TF 卡独立目录,避免和 /sdcard/xiaozhi_ui 资源目录混在根目录。
#define FACE_DB_DIR "/sdcard/xiaozhi_face"
#define FACE_DB_PATH FACE_DB_DIR "/face.db"
#define FACE_DB_LEGACY_PATH "/sdcard/face.db"

namespace smart_home {

static bool face_db_storage_ready()
{
    struct stat st = {};
    if (stat("/sdcard", &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "Face DB storage is not ready: /sdcard not mounted");
        return false;
    }

    if (stat(FACE_DB_DIR, &st) != 0) {
        if (mkdir(FACE_DB_DIR, 0775) != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create face DB dir %s (errno=%d)", FACE_DB_DIR, errno);
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "Face DB path is not a directory: %s", FACE_DB_DIR);
        return false;
    }

    struct stat new_db = {};
    struct stat old_db = {};
    if (stat(FACE_DB_PATH, &new_db) != 0 && stat(FACE_DB_LEGACY_PATH, &old_db) == 0) {
        if (rename(FACE_DB_LEGACY_PATH, FACE_DB_PATH) == 0) {
            ESP_LOGI(TAG, "Migrated face DB: %s -> %s", FACE_DB_LEGACY_PATH, FACE_DB_PATH);
        } else {
            ESP_LOGW(TAG, "Failed to migrate legacy face DB (errno=%d), using %s",
                     errno, FACE_DB_PATH);
        }
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

bool FaceRecognition::InitializeDetector()
{
    if (detector_) {
        return true;
    }

    try {
        auto *detector = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);
        detector_ = detector;
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Failed to create HumanFaceDetect: %s", e.what());
        return false;
    }

    ESP_LOGI(TAG, "Face detector initialized");
    return true;
}

bool FaceRecognition::Initialize()
{
    if (initialized_) {
        return true;
    }

    if (!face_db_storage_ready() || !InitializeDetector()) {
        return false;
    }

    try {
        auto *recognizer = new HumanFaceRecognizer(FACE_DB_PATH,
                                                    HumanFaceFeat::MFN_S8_V1,
                                                    false);
        recognizer_ = recognizer;
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Failed to create HumanFaceRecognizer: %s", e.what());
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

static void fill_detect_results(const std::list<dl::detect::result_t> &detect_res,
                                std::vector<FaceDetectResult> &results)
{
    results.clear();
    for (const auto &r : detect_res) {
        FaceDetectResult fr;
        fr.x = r.box[0];
        fr.y = r.box[1];
        fr.width = r.box[2] - r.box[0];
        fr.height = r.box[3] - r.box[1];
        fr.score = r.score;
        results.push_back(fr);
    }
}

bool FaceRecognition::DetectFaces(const uint8_t *data, int width, int height,
                                   std::vector<FaceDetectResult> &results)
{
    if (!detector_) {
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
    fill_detect_results(detect_res, results);

    heap_caps_free(rgb888);
    return found;
}

bool FaceRecognition::DetectAndRecognize(const uint8_t *data, int width, int height,
                                          std::vector<FaceDetectResult> &detect_results,
                                          FaceRecognizeResult &result)
{
    detect_results.clear();
    result = FaceRecognizeResult{};

    if (!initialized_ || !detector_ || !recognizer_) {
        result.error_msg = "Not initialized";
        return false;
    }

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

    auto *detector = static_cast<HumanFaceDetect *>(detector_);
    std::list<dl::detect::result_t> &detect_res = detector->run(img);
    fill_detect_results(detect_res, detect_results);

    if (detect_res.empty()) {
        result.error_msg = "No face detected";
        heap_caps_free(rgb888);
        return false;
    }

    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    std::vector<dl::recognition::result_t> recog_res = recognizer->recognize(img, detect_res);
    heap_caps_free(rgb888);

    if (recog_res.empty()) {
        result.error_msg = "No match in database";
        return false;
    }

    result.id = recog_res[0].id;
    result.similarity = recog_res[0].similarity;
    result.success = true;
    return true;
}

bool FaceRecognition::DetectAndRegister(const uint8_t *data, int width, int height,
                                         std::vector<FaceDetectResult> &detect_results,
                                         const std::string &user_name,
                                         std::string &user_id)
{
    (void)user_name;
    detect_results.clear();

    if (!initialized_ || !detector_ || !recognizer_) {
        return false;
    }

    uint8_t *rgb888 = rgb565_to_rgb888(data, width, height);
    if (!rgb888) {
        return false;
    }

    dl::image::img_t img = {};
    img.data = rgb888;
    img.width = (uint16_t)width;
    img.height = (uint16_t)height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

    auto *detector = static_cast<HumanFaceDetect *>(detector_);
    std::list<dl::detect::result_t> &detect_res = detector->run(img);
    fill_detect_results(detect_res, detect_results);

    if (detect_res.empty()) {
        heap_caps_free(rgb888);
        return false;
    }

    auto *recognizer = static_cast<HumanFaceRecognizer *>(recognizer_);
    esp_err_t ret = recognizer->enroll(img, detect_res);
    heap_caps_free(rgb888);

    if (ret != ESP_OK) {
        return false;
    }

    user_id = std::to_string(recognizer->get_num_feats());
    return true;
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
