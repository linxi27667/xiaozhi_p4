#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace smart_home {

// 人脸检测结果(单张人脸的边界框)
struct FaceDetectResult {
    int x = 0;          // 边界框左上角 X
    int y = 0;          // 边界框左上角 Y
    int width = 0;      // 边界框宽度
    int height = 0;     // 边界框高度
    float score = 0.0f; // 置信度
};

// 人脸识别结果
struct FaceRecognizeResult {
    bool success = false;        // 是否识别成功(数据库中存在匹配)
    uint16_t id = 0;             // 数据库中的 ID
    float similarity = 0.0f;     // 相似度
    std::string error_msg;       // 错误信息
};

// 人脸识别引擎封装
// 基于 esp-dl 的 HumanFaceDetect + HumanFaceRecognizer
// 人脸特征数据库使用 HumanFaceRecognizer 自带的 DataBase(基于 FAT 文件系统)
class FaceRecognition {
public:
    static FaceRecognition &GetInstance();

    // 初始化引擎(加载模型,打开数据库)
    // 需要先挂载 FAT 文件系统分区
    bool Initialize();

    // 是否已初始化
    bool IsInitialized() const { return initialized_; }

    // 人脸检测:返回检测到的人脸列表
    // data: RGB565 或 RGB888 图像数据
    // 返回 false 表示未检测到人脸或发生错误
    bool DetectFaces(const uint8_t *data, int width, int height,
                     std::vector<FaceDetectResult> &results);

    // 人脸识别:对检测到的人脸进行识别
    // 返回 true 表示识别成功(数据库中有匹配)
    bool RecognizeFace(const uint8_t *data, int width, int height,
                       const FaceDetectResult &detect,
                       FaceRecognizeResult &result);

    // 注册人脸:采集当前帧,提取特征,保存到数据库
    // 返回 true 表示注册成功
    bool RegisterFace(const uint8_t *data, int width, int height,
                      const FaceDetectResult &detect,
                      const std::string &user_name,
                      std::string &user_id);

    // 获取已注册人脸数量
    int GetFaceCount();

    // 清空所有人脸
    bool ClearAllFaces();

    // 删除指定 ID 的人脸
    bool DeleteFace(uint16_t id);

private:
    FaceRecognition() = default;
    ~FaceRecognition();
    FaceRecognition(const FaceRecognition &) = delete;
    FaceRecognition &operator=(const FaceRecognition &) = delete;

    bool initialized_ = false;
    void *detector_ = nullptr;       // HumanFaceDetect*
    void *recognizer_ = nullptr;     // HumanFaceRecognizer*
};

} // namespace smart_home
