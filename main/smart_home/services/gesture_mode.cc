#include "gesture_mode.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

#include "application.h"
#include "board.h"
#include "display.h"
#include "esp_video.h"
#include "gesture_filter.h"
#include "gesture_runtime_policy.h"
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"
#include "smart_home/services/xiaozhi_mqtt.h"
#include "smart_home/ui/model/mqtt_device_model.h"
#include "smart_home/ui/services/gesture_ui.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

#ifndef CONFIG_PIX_CVT_RGB888_TO_RGB888_SUPPORT
#error "Gesture mode requires CONFIG_PIX_CVT_RGB888_TO_RGB888_SUPPORT"
#endif

namespace {

constexpr char kTag[] = "GestureMode";
constexpr uint32_t kMinimumInterFrameYieldMs = 25;
constexpr uint32_t kPausePollMs = 50;
constexpr size_t kWorkerStackBytes = 16 * 1024;
constexpr UBaseType_t kWorkerPriority = 1;
constexpr size_t kPreviewBytes = GESTURE_PREVIEW_BYTES;
constexpr size_t kInferenceBytes =
    static_cast<size_t>(GESTURE_PREVIEW_WIDTH) * GESTURE_PREVIEW_HEIGHT * 3;

void ConvertRgb565LeToRgb888(const uint8_t *source, uint8_t *destination,
                            size_t pixel_count) {
    const auto *source_pixels = reinterpret_cast<const uint16_t *>(source);
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint16_t pixel = source_pixels[i];
        const uint8_t red = static_cast<uint8_t>((pixel >> 11) & 0x1F);
        const uint8_t green = static_cast<uint8_t>((pixel >> 5) & 0x3F);
        const uint8_t blue = static_cast<uint8_t>(pixel & 0x1F);
        destination[i * 3] = static_cast<uint8_t>((red << 3) | (red >> 2));
        destination[i * 3 + 1] =
            static_cast<uint8_t>((green << 2) | (green >> 4));
        destination[i * 3 + 2] =
            static_cast<uint8_t>((blue << 3) | (blue >> 2));
    }
}

uint64_t NowMs() {
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

uint8_t ConfirmationFramesForCategory(const char *category) {
    if (category == nullptr) {
        return 3;
    }
    if (std::strcmp(category, "no_gesture") == 0) {
        return GestureVotesRequired(GestureClass::Fist);
    }
    if (std::strcmp(category, "two") == 0) {
        return GestureVotesRequired(GestureClass::Two);
    }
    if (std::strcmp(category, "three") == 0) {
        return GestureVotesRequired(GestureClass::Three);
    }
    if (std::strcmp(category, "four") == 0) {
        return GestureVotesRequired(GestureClass::Four);
    }
    return 3;
}

void CopyText(char *destination, size_t length, const char *source) {
    if (destination == nullptr || length == 0) {
        return;
    }
    std::snprintf(destination, length, "%s", source != nullptr ? source : "");
}

void LogHeap(const char *stage) {
    ESP_LOGI(kTag,
             "Heap [%s]: internal=%u internal_largest=%u psram=%u psram_largest=%u",
             stage,
             static_cast<unsigned>(
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
}

class GestureModeService {
public:
    static GestureModeService &Instance() {
        static GestureModeService instance;
        return instance;
    }

    bool Initialize() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (worker_task_ != nullptr) {
            return true;
        }

        const BaseType_t result = xTaskCreatePinnedToCoreWithCaps(
            WorkerEntry, "gesture_worker", kWorkerStackBytes, this, kWorkerPriority,
            &worker_task_, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (result != pdPASS) {
            worker_task_ = nullptr;
            snapshot_.state = GESTURE_STATE_ERROR;
            CopyText(snapshot_.error, sizeof(snapshot_.error), "无法创建手势任务");
            ESP_LOGE(kTag, "Failed to create gesture worker");
            return false;
        }
        snapshot_.state = GESTURE_STATE_STOPPED;
        CopyText(snapshot_.status, sizeof(snapshot_.status), "手控模式未开启");
        ESP_LOGI(kTag,
                 "Worker created on core 1, priority %u, INTERNAL stack %u bytes "
                 "(required for Flash cache-off model loading)",
                 static_cast<unsigned>(kWorkerPriority),
                 static_cast<unsigned>(kWorkerStackBytes));
        LogHeap("worker-created");
        return true;
    }

    bool Start(gesture_start_source_t source) {
        if (!Initialize()) {
            return false;
        }

        const DeviceState device_state = Application::GetInstance().GetDeviceState();
        const mqtt_device_model_t *model = device_model_get();
        if (device_state == kDeviceStateLocked ||
            device_state == kDeviceStateStarting ||
            device_state == kDeviceStateUpgrading ||
            device_state == kDeviceStateFatalError ||
            (model != nullptr && model->current_scene == IOT_SCENE_FIRE)) {
            SetStartError("当前为锁屏、升级或报警状态，不能开启");
            return false;
        }

        auto *camera = dynamic_cast<EspVideo *>(Board::GetInstance().GetCamera());
        if (camera == nullptr || !camera->IsReady()) {
            SetStartError("图像不可用");
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (requested_active_.load(std::memory_order_acquire) ||
                (snapshot_.state != GESTURE_STATE_STOPPED &&
                 snapshot_.state != GESTURE_STATE_ERROR)) {
                CopyText(snapshot_.error, sizeof(snapshot_.error), "手控模式正在运行或关闭");
                return false;
            }
            requested_active_.store(true, std::memory_order_release);
            stop_reason_ = GESTURE_STOP_USER;
            snapshot_ = {};
            snapshot_.active = true;
            snapshot_.state = GESTURE_STATE_PAUSED;
            snapshot_.timeout_seconds = 0;
            CopyText(snapshot_.status, sizeof(snapshot_.status),
                     "小智关闭中...");
        }

        // The assistant finishes its current sentence and closes the online
        // audio channel before the worker is allowed to allocate/load ESP-DL.
        ScheduleUi(true);
        Application::GetInstance().RequestGestureExclusiveMode();
        xTaskNotifyGive(worker_task_);
        ESP_LOGI(kTag, "Start requested by %s",
                 source == GESTURE_START_TOUCH ? "touch" : "MCP");
        return true;
    }

    void Stop(gesture_stop_reason_t reason) {
        const bool was_active =
            requested_active_.exchange(false, std::memory_order_acq_rel);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            stop_reason_ = reason;
            snapshot_.active = false;
            snapshot_.timeout_seconds = 0;
            if (snapshot_.state != GESTURE_STATE_STOPPED &&
                snapshot_.state != GESTURE_STATE_ERROR) {
                snapshot_.state = GESTURE_STATE_STOPPING;
                CopyText(snapshot_.status, sizeof(snapshot_.status),
                         "正在关闭手控模式...");
            } else if (snapshot_.state == GESTURE_STATE_STOPPED) {
                CopyText(snapshot_.status, sizeof(snapshot_.status),
                         "手控模式已关闭");
            }
        }
        // Stop is intentionally idempotent. A second MCP/touch request must
        // still remove a stale overlay and return the authoritative state.
        ScheduleUi(false);
        if (worker_task_ != nullptr) {
            xTaskNotifyGive(worker_task_);
        }
        ESP_LOGI(kTag, "Stop requested, reason=%d, was_active=%s",
                 static_cast<int>(reason), was_active ? "true" : "false");
    }

    bool IsActive() const {
        return requested_active_.load(std::memory_order_acquire);
    }

    gesture_mode_state_t State() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return snapshot_.state;
    }

    bool Snapshot(gesture_mode_snapshot_t *snapshot) {
        if (snapshot == nullptr) {
            return false;
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        *snapshot = snapshot_;
        snapshot->timeout_seconds = 0;
        return true;
    }

    bool CopyPreview(uint8_t *output, size_t output_len, uint32_t *sequence) {
        if (output == nullptr || output_len < kPreviewBytes || sequence == nullptr) {
            return false;
        }

        std::lock_guard<std::mutex> lock(preview_mutex_);
        if (latest_preview_ == nullptr || *sequence == preview_sequence_) {
            return false;
        }

        const auto *source = reinterpret_cast<const uint16_t *>(latest_preview_);
        auto *destination = reinterpret_cast<uint16_t *>(output);
        for (int y = 0; y < GESTURE_PREVIEW_HEIGHT; ++y) {
            const size_t row = static_cast<size_t>(y) * GESTURE_PREVIEW_WIDTH;
            for (int x = 0; x < GESTURE_PREVIEW_WIDTH; ++x) {
                destination[row + x] = source[row + (GESTURE_PREVIEW_WIDTH - 1 - x)];
            }
        }
        *sequence = preview_sequence_;
        return true;
    }

private:
    GestureModeService() {
        snapshot_.state = GESTURE_STATE_STOPPED;
    }

    ~GestureModeService() = default;
    GestureModeService(const GestureModeService &) = delete;
    GestureModeService &operator=(const GestureModeService &) = delete;

    static void WorkerEntry(void *argument) {
        static_cast<GestureModeService *>(argument)->Worker();
    }

    void Worker() {
        while (true) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (!requested_active_.load(std::memory_order_acquire)) {
                FinalizeStopped();
                Application::GetInstance().ReleaseGestureExclusiveMode();
                continue;
            }

            while (requested_active_.load(std::memory_order_acquire)) {
                auto &app = Application::GetInstance();
                const DeviceState device_state = app.GetDeviceState();
                const mqtt_device_model_t *model = device_model_get();
                if (device_state == kDeviceStateLocked ||
                    device_state == kDeviceStateStarting ||
                    device_state == kDeviceStateUpgrading ||
                    device_state == kDeviceStateFatalError ||
                    (model != nullptr && model->current_scene == IOT_SCENE_FIRE)) {
                    Stop(GESTURE_STOP_CRITICAL_STATE);
                    break;
                }
                if (app.IsGestureExclusiveModeReady() &&
                    GestureModeCanRun(device_state)) {
                    SetState(GESTURE_STATE_LOADING, "模型初始化中...");
                    break;
                }
                SetState(GESTURE_STATE_PAUSED, "小智关闭中...");
                vTaskDelay(pdMS_TO_TICKS(kPausePollMs));
            }
            if (!requested_active_.load(std::memory_order_acquire)) {
                FinalizeStopped();
                Application::GetInstance().ReleaseGestureExclusiveMode();
                continue;
            }

            if (!LoadResources()) {
                requested_active_.store(false, std::memory_order_release);
                CleanupResources();
                Application::GetInstance().ReleaseGestureExclusiveMode();
                continue;
            }
            if (!requested_active_.load(std::memory_order_acquire)) {
                CleanupResources();
                FinalizeStopped();
                Application::GetInstance().ReleaseGestureExclusiveMode();
                continue;
            }

            filter_.Reset();
            SetState(GESTURE_STATE_RUNNING, "请伸手");
            ESP_LOGI(kTag,
                     "Resources ready; exclusive frame period=%u ms",
                     static_cast<unsigned>(kGestureIdleFramePeriodMs));
            uint32_t frame_sequence = 0;
            bool first_capture_request_logged = false;

            while (requested_active_.load(std::memory_order_acquire)) {
                const uint64_t frame_start_ms = NowMs();
                const uint64_t now_ms = frame_start_ms;

                auto &app = Application::GetInstance();
                const DeviceState device_state = app.GetDeviceState();
                const mqtt_device_model_t *model = device_model_get();
                if (device_state == kDeviceStateLocked ||
                    device_state == kDeviceStateStarting ||
                    device_state == kDeviceStateUpgrading ||
                    device_state == kDeviceStateFatalError ||
                    (model != nullptr && model->current_scene == IOT_SCENE_FIRE)) {
                    Stop(GESTURE_STOP_CRITICAL_STATE);
                    break;
                }
                if (!app.IsGestureExclusiveModeReady() ||
                    !GestureModeCanRun(device_state)) {
                    Fail("手控状态失效 已安全关闭");
                    break;
                }

                auto *camera = dynamic_cast<EspVideo *>(Board::GetInstance().GetCamera());
                if (camera == nullptr) {
                    Fail("摄像头不可用");
                    break;
                }

                if (!first_capture_request_logged) {
                    ESP_LOGI(kTag, "First frame: requesting camera");
                    first_capture_request_logged = true;
                }
                const uint64_t capture_start_ms = NowMs();
                const EspVideo::ScaledCaptureResult capture_result =
                    camera->TryCaptureScaledRgb565(capture_buffer_, kPreviewBytes,
                                                   GESTURE_PREVIEW_WIDTH,
                                                   GESTURE_PREVIEW_HEIGHT);
                if (capture_result == EspVideo::ScaledCaptureResult::Busy) {
                    SetState(GESTURE_STATE_PAUSED, "等待图像");
                    vTaskDelay(pdMS_TO_TICKS(kPausePollMs));
                    continue;
                }
                if (capture_result == EspVideo::ScaledCaptureResult::NotReady) {
                    SetState(GESTURE_STATE_PAUSED, "等待图像");
                    vTaskDelay(pdMS_TO_TICKS(kPausePollMs));
                    continue;
                }
                if (capture_result != EspVideo::ScaledCaptureResult::Ok) {
                    Fail("图像取帧失败");
                    break;
                }

                SetState(GESTURE_STATE_RUNNING, "请伸手");
                const uint32_t capture_ms =
                    static_cast<uint32_t>(NowMs() - capture_start_ms);
                const uint64_t inference_start_ms = NowMs();
                if (frame_sequence == 0) {
                    ESP_LOGI(kTag, "First frame: captured in %u ms; starting inference",
                             static_cast<unsigned>(capture_ms));
                }
                if (!ProcessFrame(now_ms, frame_sequence == 0)) {
                    if (!requested_active_.load(std::memory_order_acquire)) {
                        break;
                    }
                    Fail("手控状态失效 已安全关闭");
                    break;
                }
                const uint32_t inference_ms =
                    static_cast<uint32_t>(NowMs() - inference_start_ms);
                PublishPreview();

                ++frame_sequence;
                const uint32_t frame_ms =
                    static_cast<uint32_t>(NowMs() - frame_start_ms);
                if (frame_sequence == 1 || frame_sequence % 30 == 0) {
                    if (frame_ms > 1000) {
                        ESP_LOGW(kTag,
                                 "Slow frame %u: capture=%u ms inference=%u ms total=%u ms",
                                 static_cast<unsigned>(frame_sequence),
                                 static_cast<unsigned>(capture_ms),
                                 static_cast<unsigned>(inference_ms),
                                 static_cast<unsigned>(frame_ms));
                    } else {
                        ESP_LOGI(kTag,
                                 "Frame %u: capture=%u ms inference=%u ms total=%u ms",
                                 static_cast<unsigned>(frame_sequence),
                                 static_cast<unsigned>(capture_ms),
                                 static_cast<unsigned>(inference_ms),
                                 static_cast<unsigned>(frame_ms));
                    }
                }

                // Always block after a frame so LVGL and smart-home tasks keep
                // deterministic scheduling time even in the exclusive mode.
                const uint32_t frame_period_ms = GestureModeFramePeriodMs();
                const uint32_t remaining_ms =
                    frame_ms < frame_period_ms
                        ? frame_period_ms - frame_ms
                        : 0;
                const uint32_t delay_ms =
                    std::max(kMinimumInterFrameYieldMs, remaining_ms);
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
            }

            CleanupResources();
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                snapshot_.active = false;
                snapshot_.timeout_seconds = 0;
                if (snapshot_.state != GESTURE_STATE_ERROR) {
                    snapshot_.state = GESTURE_STATE_STOPPED;
                    CopyText(snapshot_.status, sizeof(snapshot_.status),
                             "手控模式已关闭");
                }
            }
            Application::GetInstance().ReleaseGestureExclusiveMode();
            ESP_LOGI(kTag, "Worker idle, stack high-water=%u bytes",
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
    }

    bool LoadResources() {
        LogHeap("before-load");
        capture_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(
            64, kPreviewBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        spare_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(
            64, kPreviewBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        inference_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(
            64, kInferenceBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (capture_buffer_ == nullptr || spare_buffer_ == nullptr ||
            inference_buffer_ == nullptr) {
            Fail("图像堆分配失败");
            return false;
        }

        ESP_LOGI(kTag,
                 "Loading PER_TENSOR models: hand_detect=0.1.1, "
                 "hand_gesture_recognition=0.1.2");
        try {
            detector_ = std::make_unique<HandDetect>(
                HandDetect::ESPDET_PICO_224_224_HAND, false);
            detector_->set_score_thr(kGestureDetectionThreshold, 0);
            classifier_ = std::make_unique<HandGestureCls>(
                HandGestureCls::MOBILENETV2_0_5_S8_V1, false);
        } catch (...) {
            Fail("手控模型初始化失败");
            return false;
        }
        if (detector_ == nullptr || classifier_ == nullptr) {
            Fail("手控模型初始化失败");
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(preview_mutex_);
            latest_preview_ = nullptr;
            preview_sequence_ = 0;
        }
        last_observation_log_ms_ = 0;
        LogHeap("after-load");
        return true;
    }

    void CleanupResources() {
        classifier_.reset();
        detector_.reset();
        {
            std::lock_guard<std::mutex> lock(preview_mutex_);
            latest_preview_ = nullptr;
            preview_sequence_ = 0;
        }
        if (capture_buffer_ != nullptr) {
            heap_caps_free(capture_buffer_);
            capture_buffer_ = nullptr;
        }
        if (spare_buffer_ != nullptr) {
            heap_caps_free(spare_buffer_);
            spare_buffer_ = nullptr;
        }
        if (inference_buffer_ != nullptr) {
            heap_caps_free(inference_buffer_);
            inference_buffer_ = nullptr;
        }
        filter_.Reset();
        LogHeap("after-cleanup");
    }

    bool ProcessFrame(uint64_t now_ms, bool log_stages) {
        const uint64_t conversion_start_ms = NowMs();
        ConvertRgb565LeToRgb888(
            capture_buffer_, inference_buffer_,
            static_cast<size_t>(GESTURE_PREVIEW_WIDTH) * GESTURE_PREVIEW_HEIGHT);
        dl::image::img_t image = {
            .data = inference_buffer_,
            .width = GESTURE_PREVIEW_WIDTH,
            .height = GESTURE_PREVIEW_HEIGHT,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888,
        };

        if (log_stages) {
            ESP_LOGI(kTag, "First frame: RGB565->RGB888 conversion=%u ms; starting detector",
                     static_cast<unsigned>(NowMs() - conversion_start_ms));
        }
        if (!requested_active_.load(std::memory_order_acquire)) {
            return false;
        }
        const uint64_t detector_start_ms = NowMs();
        auto &detections = detector_->run(image);
        if (log_stages) {
            ESP_LOGI(kTag, "First frame: detector=%u ms, candidates=%u",
                     static_cast<unsigned>(NowMs() - detector_start_ms),
                     static_cast<unsigned>(detections.size()));
        }
        auto &app = Application::GetInstance();
        if (!requested_active_.load(std::memory_order_acquire) ||
            !app.IsGestureExclusiveModeReady() ||
            !GestureModeCanRun(app.GetDeviceState())) {
            return false;
        }
        const dl::detect::result_t *selected = nullptr;
        int selected_area = -1;
        int64_t selected_distance = INT64_MAX;
        for (const auto &detection : detections) {
            if (!GestureDetectionAccepted(detection.score) || detection.box.size() < 4) {
                continue;
            }
            const int area = std::max(0, detection.box_area());
            const int center_x = (detection.box[0] + detection.box[2]) / 2;
            const int center_y = (detection.box[1] + detection.box[3]) / 2;
            const int64_t dx = center_x - GESTURE_PREVIEW_WIDTH / 2;
            const int64_t dy = center_y - GESTURE_PREVIEW_HEIGHT / 2;
            const int64_t distance = dx * dx + dy * dy;
            if (area > selected_area || (area == selected_area && distance < selected_distance)) {
                selected = &detection;
                selected_area = area;
                selected_distance = distance;
            }
        }

        const char *classification_name = nullptr;
        float classification_score = 0.0f;
        const bool detected_hand = selected != nullptr;
        if (selected != nullptr) {
            const uint64_t classifier_start_ms = NowMs();
            if (log_stages) {
                ESP_LOGI(kTag, "First frame: starting classifier");
            }
            auto classifications = classifier_->run_crop(image, selected->box);
            if (log_stages) {
                ESP_LOGI(kTag, "First frame: classifier=%u ms, results=%u",
                         static_cast<unsigned>(NowMs() - classifier_start_ms),
                         static_cast<unsigned>(classifications.size()));
            }
            if (!classifications.empty()) {
                classification_name = classifications.front().cat_name;
                classification_score = classifications.front().score;
            }
        }
        if (!requested_active_.load(std::memory_order_acquire) ||
            !app.IsGestureExclusiveModeReady() ||
            !GestureModeCanRun(app.GetDeviceState())) {
            return false;
        }

        const GestureObservation observation =
            NormalizeGestureObservation(classification_name, classification_score, detected_hand);
        const GestureFilterResult filter_result =
            filter_.Process(observation.gesture, observation.hand_present, now_ms);
        const uint8_t confirmation_required =
            ConfirmationFramesForCategory(classification_name);

        if (selected != nullptr &&
            now_ms - last_observation_log_ms_ >= 1000) {
            ESP_LOGI(kTag,
                     "Observed hand: detect=%.2f class=%s score=%.2f stable=%u/%u",
                     selected->score,
                     classification_name != nullptr ? classification_name : "none",
                     classification_score,
                     static_cast<unsigned>(filter_result.confirmation_count),
                     static_cast<unsigned>(confirmation_required));
            last_observation_log_ms_ = now_ms;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            CopyText(snapshot_.gesture, sizeof(snapshot_.gesture),
                     classification_name != nullptr ? classification_name : "");
            snapshot_.confidence = classification_score;
            snapshot_.confirmation_count = filter_result.confirmation_count;
            snapshot_.confirmation_required = confirmation_required;
            if (selected != nullptr) {
                snapshot_.box_x = selected->box[0];
                snapshot_.box_y = selected->box[1];
                snapshot_.box_width = selected->box[2] - selected->box[0];
                snapshot_.box_height = selected->box[3] - selected->box[1];
            } else {
                snapshot_.box_x = 0;
                snapshot_.box_y = 0;
                snapshot_.box_width = 0;
                snapshot_.box_height = 0;
            }
        }

        if (filter_result.triggered != GestureClass::None && CanExecuteGesture()) {
            ExecuteGesture(filter_result.triggered);
        }
        return true;
    }

    void ExecuteGesture(GestureClass gesture) {
        uint8_t scene = IOT_SCENE_NONE;
        bool door_command = false;
        uint8_t door_angle = 0;
        const char *action = "";
        switch (gesture) {
            case GestureClass::Fist:
                scene = IOT_SCENE_AWAY;
                action = "离家场景";
                break;
            case GestureClass::Ok:
                scene = IOT_SCENE_MOVIE;
                action = "观影场景";
                break;
            case GestureClass::Five:
                scene = IOT_SCENE_HOME;
                action = "回家场景";
                break;
            case GestureClass::Two:
                door_command = true;
                door_angle = 135;
                action = "大门已打开";
                break;
            case GestureClass::Three:
                door_command = true;
                door_angle = 0;
                action = "大门已关闭";
                break;
            case GestureClass::Four:
                scene = IOT_SCENE_BRIGHT;
                action = "明亮场景";
                break;
            default:
                return;
        }

        bool control_succeeded = true;
        if (door_command) {
            control_succeeded =
                mqtt_send_command(1, IOT_CMD_SET_SERVO, 6, door_angle);
        } else {
            mqtt_send_scene(scene);
        }
        if (!control_succeeded) {
            action = door_angle > 0 ? "开门失败" : "关门失败";
        }
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            CopyText(snapshot_.last_action, sizeof(snapshot_.last_action), action);
            CopyText(snapshot_.status, sizeof(snapshot_.status),
                     control_succeeded ? "控制成功 请收手" : "控制失败");
        }
        ESP_LOGI(kTag, "Gesture %s triggered %s", GestureClassName(gesture), action);
    }

    void PublishPreview() {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        std::swap(capture_buffer_, spare_buffer_);
        latest_preview_ = spare_buffer_;
        ++preview_sequence_;
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        snapshot_.preview_sequence = preview_sequence_;
    }

    void SetState(gesture_mode_state_t state, const char *status) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            changed = snapshot_.state != state;
            snapshot_.active = requested_active_.load(std::memory_order_acquire);
            snapshot_.state = state;
            CopyText(snapshot_.status, sizeof(snapshot_.status), status);
        }
        if (changed) {
            ESP_LOGI(kTag, "State -> %s (%s)", gesture_mode_state_name(state), status);
        }
    }

    void FinalizeStopped() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        snapshot_.active = false;
        snapshot_.timeout_seconds = 0;
        if (snapshot_.state != GESTURE_STATE_ERROR) {
            snapshot_.state = GESTURE_STATE_STOPPED;
            CopyText(snapshot_.status, sizeof(snapshot_.status),
                     "手控模式已关闭");
        }
    }

    bool CanExecuteGesture() const {
        auto &app = Application::GetInstance();
        if (!requested_active_.load(std::memory_order_acquire) ||
            !app.IsGestureExclusiveModeReady() ||
            !GestureModeCanRun(app.GetDeviceState())) {
            return false;
        }
        const mqtt_device_model_t *model = device_model_get();
        return model == nullptr || model->current_scene != IOT_SCENE_FIRE;
    }

    void SetStartError(const char *error) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        snapshot_.active = false;
        snapshot_.state = GESTURE_STATE_ERROR;
        CopyText(snapshot_.error, sizeof(snapshot_.error), error);
        CopyText(snapshot_.status, sizeof(snapshot_.status), "手控模式启动失败");
    }

    void Fail(const char *error) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            snapshot_.active = false;
            snapshot_.state = GESTURE_STATE_ERROR;
            CopyText(snapshot_.error, sizeof(snapshot_.error), error);
            CopyText(snapshot_.status, sizeof(snapshot_.status), "手控模式失败");
            stop_reason_ = GESTURE_STOP_ERROR;
        }
        requested_active_.store(false, std::memory_order_release);
        ScheduleUi(false);
        ESP_LOGE(kTag, "%s", error);
    }

    static void ScheduleUi(bool show) {
        Application::GetInstance().Schedule([show]() {
            DisplayLockGuard lock(Board::GetInstance().GetDisplay());
            if (show) {
                gesture_ui_show();
            } else {
                gesture_ui_hide();
            }
        });
    }

    mutable std::mutex state_mutex_;
    std::mutex preview_mutex_;
    std::atomic_bool requested_active_{false};
    TaskHandle_t worker_task_ = nullptr;
    gesture_mode_snapshot_t snapshot_{};
    gesture_stop_reason_t stop_reason_ = GESTURE_STOP_USER;
    GestureStabilityFilter filter_;
    std::unique_ptr<HandDetect> detector_;
    std::unique_ptr<HandGestureCls> classifier_;
    uint8_t *capture_buffer_ = nullptr;
    uint8_t *spare_buffer_ = nullptr;
    uint8_t *inference_buffer_ = nullptr;
    uint8_t *latest_preview_ = nullptr;
    uint32_t preview_sequence_ = 0;
    uint64_t last_observation_log_ms_ = 0;
};

}  // namespace

extern "C" bool gesture_mode_init(void) {
    return GestureModeService::Instance().Initialize();
}

extern "C" bool gesture_mode_start(gesture_start_source_t source) {
    return GestureModeService::Instance().Start(source);
}

extern "C" void gesture_mode_stop(gesture_stop_reason_t reason) {
    GestureModeService::Instance().Stop(reason);
}

extern "C" bool gesture_mode_is_active(void) {
    return GestureModeService::Instance().IsActive();
}

extern "C" gesture_mode_state_t gesture_mode_get_state(void) {
    return GestureModeService::Instance().State();
}

extern "C" const char *gesture_mode_state_name(gesture_mode_state_t state) {
    switch (state) {
        case GESTURE_STATE_STOPPED:
            return "stopped";
        case GESTURE_STATE_LOADING:
            return "loading";
        case GESTURE_STATE_RUNNING:
            return "running";
        case GESTURE_STATE_PAUSED:
            return "paused";
        case GESTURE_STATE_STOPPING:
            return "stopping";
        case GESTURE_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

extern "C" bool gesture_mode_get_snapshot(gesture_mode_snapshot_t *snapshot) {
    return GestureModeService::Instance().Snapshot(snapshot);
}

extern "C" bool gesture_mode_copy_preview(
    uint8_t *output, size_t output_len, uint32_t *sequence) {
    return GestureModeService::Instance().CopyPreview(output, output_len, sequence);
}
