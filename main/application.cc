#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "assets/lang_config.h"
#include "mcp_server.h"
#include "assets.h"
#include "settings.h"
#include "smart_home_tasks.h"
#include "smart_home_mcp_tool.h"
#include "smart_home/mcp/face_mcp_tool.h"
#include "smart_home/ui/model/mqtt_device_model.h"
#include "smart_home/ui/core/ui_events.h"
#include "smart_home/ui/services/login_ui.h"
#include "smart_home/services/face_recognition.h"
#include "smart_home/services/xiaozhi_mqtt.h"
#include "mqtt_iot_protocol.h"
#include "esp_video.h"

#include <cstring>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/idf_additions.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>
#include <font_awesome.h>
#include <linux/videodev2.h>

#define TAG "Application"


Application::Application() {
    event_group_ = xEventGroupCreate();

#if CONFIG_USE_DEVICE_AEC && CONFIG_USE_SERVER_AEC
#error "CONFIG_USE_DEVICE_AEC and CONFIG_USE_SERVER_AEC cannot be enabled at the same time"
#elif CONFIG_USE_DEVICE_AEC
    aec_mode_ = kAecOnDeviceSide;
#elif CONFIG_USE_SERVER_AEC
    aec_mode_ = kAecOnServerSide;
#else
    aec_mode_ = kAecOff;
#endif

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            xEventGroupSetBits(app->event_group_, MAIN_EVENT_CLOCK_TICK);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    vEventGroupDelete(event_group_);
}

bool Application::SetDeviceState(DeviceState state) {
    return state_machine_.TransitionTo(state);
}

// ============================================================================
// 登录流程集成
// ============================================================================

// 登录事件桥接:将 C 事件转换为 Application 任务调度
static void on_login_success_event(void *user_data)
{
    (void)user_data;
    ESP_LOGI(TAG, "Login success event received, scheduling device unlock");
    Application::GetInstance().Schedule([]() {
        Application::GetInstance().OnLoginSuccess();
    });
}

static void on_login_failed_event(void *user_data)
{
    (void)user_data;
    // 登录失败时不需要状态转换,UI 会显示错误提示
}

static constexpr int FACE_PREVIEW_W = 420;
static constexpr int FACE_PREVIEW_H = 315;
static std::atomic_bool s_face_login_pending{false};
static std::atomic_bool s_face_preview_pending{false};
static std::atomic_bool s_face_success_ui_ready{false};

static uint8_t *scale_rgb565_frame(const EspVideo::CapturedFrame &src, int dst_w, int dst_h)
{
    if (!src.data || src.width <= 0 || src.height <= 0 || src.format != V4L2_PIX_FMT_RGB565) {
        return nullptr;
    }

    const size_t dst_len = (size_t)dst_w * dst_h * 2;
    auto *dst = (uint8_t *)heap_caps_malloc(dst_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst) {
        ESP_LOGE(TAG, "Failed to allocate scaled face frame (%u bytes)", (unsigned)dst_len);
        return nullptr;
    }

    const auto *src16 = reinterpret_cast<const uint16_t *>(src.data);
    auto *dst16 = reinterpret_cast<uint16_t *>(dst);
    for (int y = 0; y < dst_h; ++y) {
        int sy = (int)((int64_t)y * src.height / dst_h);
        for (int x = 0; x < dst_w; ++x) {
            int sx = (int)((int64_t)x * src.width / dst_w);
            dst16[y * dst_w + x] = src16[sy * src.width + sx];
        }
    }
    return dst;
}

static uint8_t *copy_face_frame_mirrored(const uint8_t *src)
{
    const size_t len = (size_t)FACE_PREVIEW_W * FACE_PREVIEW_H * 2;
    auto *dst = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst) {
        return nullptr;
    }
    const auto *src16 = reinterpret_cast<const uint16_t *>(src);
    auto *dst16 = reinterpret_cast<uint16_t *>(dst);
    for (int y = 0; y < FACE_PREVIEW_H; ++y) {
        const uint16_t *row = src16 + y * FACE_PREVIEW_W;
        uint16_t *out = dst16 + y * FACE_PREVIEW_W;
        for (int x = 0; x < FACE_PREVIEW_W; ++x) {
            out[x] = row[FACE_PREVIEW_W - 1 - x];
        }
    }
    return dst;
}

// 演示模式人脸登录任务:检测到人脸后立即解锁
static void face_recognition_task(void *arg)
{
    (void)arg;
    uint8_t stack_probe = 0;
    ESP_LOGI(TAG, "Face detection login task started (FACE_FIX_V3 FACE_STACK_INTERNAL_V2 stack=%p)",
             &stack_probe);

    auto &face_recog = smart_home::FaceRecognition::GetInstance();
    bool face_init_ok = face_recog.InitializeDetector();
    if (!face_init_ok) {
        ESP_LOGW(TAG, "Face detector init failed, will retry");
    }

    const TickType_t frame_interval = pdMS_TO_TICKS(400);
    const TickType_t detect_interval = pdMS_TO_TICKS(900);
    const TickType_t detector_retry_interval = pdMS_TO_TICKS(2000);
    const TickType_t unlock_dispatch_timeout = pdMS_TO_TICKS(3000);
    const TickType_t face_success_ui_timeout = pdMS_TO_TICKS(600);
    const TickType_t face_success_hold_time = pdMS_TO_TICKS(250);
    int no_face_count = 0;
    const int max_no_face = 4;  // 约4秒无人脸则提示
    bool preview_started = false;
    bool camera_wait_status_shown = false;
    TickType_t last_detect_tick = 0;
    TickType_t last_detector_init_attempt = xTaskGetTickCount();
    TickType_t face_login_queued_at = 0;

    while (true) {
        // 只在登录 UI 可见且处于人脸模式时工作
        if (!login_ui_is_face_mode()) {
            s_face_login_pending.store(false, std::memory_order_release);
            s_face_success_ui_ready.store(false, std::memory_order_release);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (s_face_login_pending.load(std::memory_order_acquire)) {
            TickType_t now = xTaskGetTickCount();
            if (face_login_queued_at != 0 &&
                (now - face_login_queued_at) >= unlock_dispatch_timeout) {
                ESP_LOGW(TAG, "Face unlock callback timed out after %lums; retrying",
                         (unsigned long)((now - face_login_queued_at) * portTICK_PERIOD_MS));
                s_face_login_pending.store(false, std::memory_order_release);
                face_login_queued_at = 0;
            } else {
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            }
        }

        if (Application::GetInstance().GetDeviceState() != kDeviceStateLocked) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // 获取摄像头
        auto *camera = Board::GetInstance().GetCamera();
        if (!camera) {
            ESP_LOGW(TAG, "No camera available");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 转换为 EspVideo 以使用 CaptureFrame
        EspVideo *esp_video = dynamic_cast<EspVideo *>(camera);
        if (!esp_video) {
            ESP_LOGW(TAG, "Camera is not EspVideo");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!esp_video->IsStreaming()) {
            if (!camera_wait_status_shown) {
                Application::GetInstance().Schedule([]() {
                    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                    if (login_ui_is_face_mode()) {
                        login_ui_set_status("启动中...");
                    }
                });
                camera_wait_status_shown = true;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        camera_wait_status_shown = false;

        if (!face_init_ok) {
            TickType_t retry_now = xTaskGetTickCount();
            if ((retry_now - last_detector_init_attempt) >= detector_retry_interval) {
                last_detector_init_attempt = retry_now;
                face_init_ok = face_recog.InitializeDetector();
                if (face_init_ok) {
                    ESP_LOGI(TAG, "Face detector recovered after retry");
                }
            }
            if (!face_init_ok) {
                vTaskDelay(frame_interval);
                continue;
            }
        }

        // 采集一帧并缩放到登录预览/模型输入尺寸
        EspVideo::CapturedFrame frame;
        if (!esp_video->CaptureFrame(frame)) {
            ESP_LOGD(TAG, "CaptureFrame failed");
            vTaskDelay(frame_interval);
            continue;
        }

        if (frame.format != V4L2_PIX_FMT_RGB565) {
            ESP_LOGD(TAG, "Frame format not RGB565: 0x%x", (unsigned)frame.format);
            free(frame.data);
            vTaskDelay(frame_interval);
            continue;
        }

        uint8_t *face_frame = scale_rgb565_frame(frame, FACE_PREVIEW_W, FACE_PREVIEW_H);
        free(frame.data);
        if (!face_frame) {
            vTaskDelay(frame_interval);
            continue;
        }

        bool preview_expected_idle = false;
        if (s_face_preview_pending.compare_exchange_strong(
                preview_expected_idle, true, std::memory_order_acq_rel)) {
            uint8_t *preview_frame = copy_face_frame_mirrored(face_frame);
            if (preview_frame) {
                try {
                    Application::GetInstance().Schedule([preview_frame]() {
                        {
                            DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                            if (login_ui_is_face_mode()) {
                                login_ui_update_face_preview(preview_frame, FACE_PREVIEW_W, FACE_PREVIEW_H);
                            }
                        }
                        free(preview_frame);
                        s_face_preview_pending.store(false, std::memory_order_release);
                    });
                    if (!preview_started) {
                        ESP_LOGI(TAG, "Face preview started (%dx%d)", FACE_PREVIEW_W, FACE_PREVIEW_H);
                        preview_started = true;
                    }
                } catch (...) {
                    free(preview_frame);
                    s_face_preview_pending.store(false, std::memory_order_release);
                    ESP_LOGE(TAG, "Failed to queue face preview frame");
                }
            } else {
                s_face_preview_pending.store(false, std::memory_order_release);
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_detect_tick) < detect_interval) {
            free(face_frame);
            vTaskDelay(frame_interval);
            continue;
        }
        last_detect_tick = now;

        std::vector<smart_home::FaceDetectResult> detect_results;
        if (!face_recog.DetectFaces(face_frame, FACE_PREVIEW_W, FACE_PREVIEW_H, detect_results)) {
            no_face_count++;
        }

        if (detect_results.empty()) {
            // 未检测到人脸
            if (no_face_count >= max_no_face) {
                Application::GetInstance().Schedule([]() {
                    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                    if (login_ui_is_face_mode()) {
                        login_ui_set_status("请面向屏幕");
                        login_ui_clear_face_detect();
                    }
                });
                no_face_count = 0;
            }
            free(face_frame);
            vTaskDelay(frame_interval);
            continue;
        }

        // 检测到人脸
        no_face_count = 0;
        auto detect = detect_results[0];
        for (const auto &candidate : detect_results) {
            if (candidate.score > detect.score) {
                detect = candidate;
            }
        }
        free(face_frame);

        DeviceState queued_state = Application::GetInstance().GetDeviceState();
        ESP_LOGI(TAG, "Face unlock queued: state=%s score=%.2f box=%d,%d %dx%d",
                 DeviceStateMachine::GetStateName(queued_state), detect.score,
                 detect.x, detect.y, detect.width, detect.height);
        s_face_login_pending.store(true, std::memory_order_release);
        s_face_success_ui_ready.store(false, std::memory_order_release);
        face_login_queued_at = xTaskGetTickCount();
        TickType_t queued_tick = face_login_queued_at;

        // 先让 LVGL 显示成功框。等待有超时兜底，不会让 UI 卡顿永久阻断解锁。
        try {
            Application::GetInstance().Schedule([detect]() {
                bool shown = false;
                {
                    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                    int preview_x = FACE_PREVIEW_W - detect.x - detect.width;
                    shown = login_ui_show_face_success(
                        preview_x, detect.y, detect.width, detect.height);
                }
                s_face_success_ui_ready.store(shown, std::memory_order_release);
            });
        } catch (...) {
            s_face_login_pending.store(false, std::memory_order_release);
            face_login_queued_at = 0;
            ESP_LOGE(TAG, "Failed to queue face success UI");
            vTaskDelay(frame_interval);
            continue;
        }

        TickType_t ui_wait_started = xTaskGetTickCount();
        while (!s_face_success_ui_ready.load(std::memory_order_acquire) &&
               (xTaskGetTickCount() - ui_wait_started) < face_success_ui_timeout) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (s_face_success_ui_ready.load(std::memory_order_acquire)) {
            ESP_LOGI(TAG, "Face success box ready; holding for %lums",
                     (unsigned long)(face_success_hold_time * portTICK_PERIOD_MS));
            vTaskDelay(face_success_hold_time);
        } else {
            ESP_LOGW(TAG, "Face success UI timed out; continuing unlock");
        }

        try {
            Application::GetInstance().Schedule([queued_tick]() {
                auto &app = Application::GetInstance();
                ESP_LOGI(TAG, "Face unlock callback begin: state=%s latency=%lums",
                         DeviceStateMachine::GetStateName(app.GetDeviceState()),
                         (unsigned long)((xTaskGetTickCount() - queued_tick) * portTICK_PERIOD_MS));

                app.OnLoginSuccess();
                DeviceState state_after = app.GetDeviceState();
                bool ui_finished = false;
                if (state_after == kDeviceStateIdle) {
                    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                    ui_finished = login_ui_finish_face_success();
                } else {
                    ESP_LOGE(TAG, "Face unlock callback did not reach idle: state=%s",
                             DeviceStateMachine::GetStateName(state_after));
                }

                ESP_LOGI(TAG, "Face unlock callback done: state=%s ui=%s",
                         DeviceStateMachine::GetStateName(state_after),
                         ui_finished ? "finished" : "skipped");
                s_face_success_ui_ready.store(false, std::memory_order_release);
                s_face_login_pending.store(false, std::memory_order_release);
            });
        } catch (...) {
            s_face_login_pending.store(false, std::memory_order_release);
            face_login_queued_at = 0;
            ESP_LOGE(TAG, "Failed to queue face unlock callback");
        }

        // 给主任务时间执行解锁回调；若回调超时，检测循环会自动重试。
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void Application::Initialize() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    // Setup the display
    auto display = board.GetDisplay();
    display->SetupUI();

    // Setup the audio service
    auto codec = board.GetAudioCodec();
    audio_service_.Initialize(codec);
    audio_service_.Start();

    AudioServiceCallbacks callbacks;
    callbacks.on_send_queue_available = [this]() {
        xEventGroupSetBits(event_group_, MAIN_EVENT_SEND_AUDIO);
    };
    callbacks.on_wake_word_detected = [this](const std::string& wake_word) {
        xEventGroupSetBits(event_group_, MAIN_EVENT_WAKE_WORD_DETECTED);
    };
    callbacks.on_vad_change = [this](bool speaking) {
        xEventGroupSetBits(event_group_, MAIN_EVENT_VAD_CHANGE);
    };
    audio_service_.SetCallbacks(callbacks);

    // Add state change listeners
    state_machine_.AddStateChangeListener([this](DeviceState old_state, DeviceState new_state) {
        xEventGroupSetBits(event_group_, MAIN_EVENT_STATE_CHANGED);
    });

    // Start the clock timer to update the status bar
    esp_timer_start_periodic(clock_timer_handle_, 1000000);

    // Add MCP common tools (only once during initialization)
    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddCommonTools();
    mcp_server.AddUserOnlyTools();
    SmartHomeMcp_RegisterTools();
    FaceMcp_RegisterTools();

    login_ui_init();
    ui_event_subscribe(UI_EVENT_LOGIN_SUCCESS, on_login_success_event, NULL);
    ui_event_subscribe(UI_EVENT_LOGIN_FAILED, on_login_failed_event, NULL);

    BaseType_t face_task_result = xTaskCreatePinnedToCoreWithCaps(
        face_recognition_task, "face_recog", 12 * 1024, nullptr, 4, nullptr, 1,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (face_task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create face detection task");
    } else {
        ESP_LOGI(TAG, "Face task stack allocated with INTERNAL capability (FACE_STACK_INTERNAL_V2)");
    }

    SetDeviceState(kDeviceStateLocked);
    login_ui_show();
    ESP_LOGI(TAG, "Login flow initialized, device locked");

    // Set network event callback for UI updates and network state handling
    board.SetNetworkEventCallback([this](NetworkEvent event, const std::string& data) {
        auto display = Board::GetInstance().GetDisplay();
        
        switch (event) {
            case NetworkEvent::Scanning:
                device_model_update_wifi_state(WIFI_STATE_SCANNING, nullptr);
                display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_DISCONNECTED);
                break;
            case NetworkEvent::Connecting: {
                device_model_update_wifi_state(WIFI_STATE_CONNECTING, data.empty() ? nullptr : data.c_str());
                if (data.empty()) {
                    // Cellular network - registering without carrier info yet
                    display->SetStatus(Lang::Strings::REGISTERING_NETWORK);
                } else {
                    // WiFi or cellular with carrier info
                    std::string msg = Lang::Strings::CONNECT_TO;
                    msg += data;
                    msg += "...";
                    display->ShowNotification(msg.c_str(), 30000);
                }
                break;
            }
            case NetworkEvent::Connected: {
                network_ready_ = true;
                device_model_update_wifi_state(WIFI_STATE_CONNECTED, data.empty() ? "ESP-Hosted" : data.c_str());
                SmartHomeTasksNotifyNetworkReady();
                std::string msg = Lang::Strings::CONNECTED_TO;
                msg += data;
                display->ShowNotification(msg.c_str(), 30000);
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_CONNECTED);
                break;
            }
            case NetworkEvent::Disconnected:
                network_ready_ = false;
                device_model_update_wifi_state(WIFI_STATE_FAILED, nullptr);
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_DISCONNECTED);
                break;
            case NetworkEvent::WifiConfigModeEnter:
                network_ready_ = false;
                // WiFi config mode enter is handled by WifiBoard internally
                break;
            case NetworkEvent::WifiConfigModeExit:
                // WiFi config mode exit is handled by WifiBoard internally
                break;
            // Cellular modem specific events
            case NetworkEvent::ModemDetecting:
                display->SetStatus(Lang::Strings::DETECTING_MODULE);
                break;
            case NetworkEvent::ModemErrorNoSim:
                Alert(Lang::Strings::ERROR, Lang::Strings::PIN_ERROR, "triangle_exclamation", Lang::Sounds::OGG_ERR_PIN);
                break;
            case NetworkEvent::ModemErrorRegDenied:
                Alert(Lang::Strings::ERROR, Lang::Strings::REG_ERROR, "triangle_exclamation", Lang::Sounds::OGG_ERR_REG);
                break;
            case NetworkEvent::ModemErrorInitFailed:
                Alert(Lang::Strings::ERROR, Lang::Strings::MODEM_INIT_ERROR, "triangle_exclamation", Lang::Sounds::OGG_EXCLAMATION);
                break;
            case NetworkEvent::ModemErrorTimeout:
                display->SetStatus(Lang::Strings::REGISTERING_NETWORK);
                break;
        }
    });

    // Start the local smart-home task layer before network bring-up so UI pages
    // remain responsive even when the ESP-Hosted WiFi slave is slow or absent.
    SmartHomeTasksStart();

    // Update the status bar immediately to show the network state
    display->UpdateStatusBar(true);

    BaseType_t network_task_ret = xTaskCreate([](void* arg) {
        (void)arg;
        ESP_LOGI(TAG, "Network start task started");
        Board::GetInstance().StartNetwork();
        ESP_LOGI(TAG, "Network start task finished");
        vTaskDelete(NULL);
    }, "network_start", 8192, nullptr, 5, nullptr);

    if (network_task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network_start task");
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
    }
}

void Application::Run() {
    // Set the priority of the main task to 10
    vTaskPrioritySet(nullptr, 10);

    const EventBits_t ALL_EVENTS = 
        MAIN_EVENT_SCHEDULE |
        MAIN_EVENT_SEND_AUDIO |
        MAIN_EVENT_WAKE_WORD_DETECTED |
        MAIN_EVENT_VAD_CHANGE |
        MAIN_EVENT_CLOCK_TICK |
        MAIN_EVENT_ERROR |
        MAIN_EVENT_NETWORK_CONNECTED |
        MAIN_EVENT_NETWORK_DISCONNECTED |
        MAIN_EVENT_TOGGLE_CHAT |
        MAIN_EVENT_START_LISTENING |
        MAIN_EVENT_STOP_LISTENING |
        MAIN_EVENT_ACTIVATION_DONE |
        MAIN_EVENT_STATE_CHANGED;

    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, ALL_EVENTS, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & MAIN_EVENT_ERROR) {
            SetDeviceState(kDeviceStateIdle);
            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        }

        if (bits & MAIN_EVENT_NETWORK_CONNECTED) {
            HandleNetworkConnectedEvent();
        }

        if (bits & MAIN_EVENT_NETWORK_DISCONNECTED) {
            HandleNetworkDisconnectedEvent();
        }

        if (bits & MAIN_EVENT_ACTIVATION_DONE) {
            HandleActivationDoneEvent();
        }

        if (bits & MAIN_EVENT_STATE_CHANGED) {
            HandleStateChangedEvent();
        }

        if (bits & MAIN_EVENT_TOGGLE_CHAT) {
            HandleToggleChatEvent();
        }

        if (bits & MAIN_EVENT_START_LISTENING) {
            HandleStartListeningEvent();
        }

        if (bits & MAIN_EVENT_STOP_LISTENING) {
            HandleStopListeningEvent();
        }

        if (bits & MAIN_EVENT_SEND_AUDIO) {
            while (auto packet = audio_service_.PopPacketFromSendQueue()) {
                if (protocol_ && !protocol_->SendAudio(std::move(packet))) {
                    break;
                }
            }
        }

        if (bits & MAIN_EVENT_WAKE_WORD_DETECTED) {
            HandleWakeWordDetectedEvent();
        }

        if (bits & MAIN_EVENT_VAD_CHANGE) {
            if (GetDeviceState() == kDeviceStateListening) {
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            }
        }

        if (bits & MAIN_EVENT_SCHEDULE) {
            std::unique_lock<std::mutex> lock(mutex_);
            auto tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }

        if (bits & MAIN_EVENT_CLOCK_TICK) {
            clock_ticks_++;
            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();
        
            // Print debug info every 10 seconds
            if (clock_ticks_ % 10 == 0) {
                SystemInfo::PrintHeapStats();
            }
        }
    }
}

void Application::HandleNetworkConnectedEvent() {
    ESP_LOGI(TAG, "Network connected");
    auto state = GetDeviceState();

    if (state == kDeviceStateStarting || state == kDeviceStateWifiConfiguring || state == kDeviceStateLocked) {
        // Network is ready, start activation
        if (state != kDeviceStateLocked) {
            SetDeviceState(kDeviceStateActivating);
        }
        if (activation_task_handle_ != nullptr) {
            ESP_LOGW(TAG, "Activation task already running");
            return;
        }

        xTaskCreate([](void* arg) {
            Application* app = static_cast<Application*>(arg);
            app->ActivationTask();
            app->activation_task_handle_ = nullptr;
            vTaskDelete(NULL);
        }, "activation", 4096 * 2, this, 2, &activation_task_handle_);
    }

    // Update the status bar immediately to show the network state
    auto display = Board::GetInstance().GetDisplay();
    display->UpdateStatusBar(true);
}

void Application::HandleNetworkDisconnectedEvent() {
    // Close current conversation when network disconnected
    auto state = GetDeviceState();
    if (state == kDeviceStateConnecting || state == kDeviceStateListening || state == kDeviceStateSpeaking) {
        ESP_LOGI(TAG, "Closing audio channel due to network disconnection");
        protocol_->CloseAudioChannel();
    }

    // Update the status bar immediately to show the network state
    auto display = Board::GetInstance().GetDisplay();
    display->UpdateStatusBar(true);
}

void Application::HandleActivationDoneEvent() {
    ESP_LOGI(TAG, "Activation done");
    activation_done_ = true;

    SystemInfo::PrintHeapStats();
    if (GetDeviceState() != kDeviceStateLocked) {
        SetDeviceState(kDeviceStateIdle);
    }

    has_server_time_ = ota_->HasServerTime();

    auto display = Board::GetInstance().GetDisplay();
    std::string message = std::string(Lang::Strings::VERSION) + ota_->GetCurrentVersion();
    display->ShowNotification(message.c_str());
    display->SetChatMessage("system", "");

    // Release OTA object after activation is complete
    ota_.reset();
    auto& board = Board::GetInstance();
    board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);

    Schedule([this]() {
        // Play the success sound to indicate the device is ready
        if (GetDeviceState() != kDeviceStateLocked) {
            audio_service_.PlaySound(Lang::Sounds::OGG_SUCCESS);
        }
    });
}

void Application::ActivationTask() {
    // Create OTA object for activation process
    ota_ = std::make_unique<Ota>();

    // Check for new assets version
    CheckAssetsVersion();

    // Check for new firmware version
    CheckNewVersion();

    // Initialize the protocol
    InitializeProtocol();

    // Signal completion to main loop
    xEventGroupSetBits(event_group_, MAIN_EVENT_ACTIVATION_DONE);
}

void Application::CheckAssetsVersion() {
    // Only allow CheckAssetsVersion to be called once
    if (assets_version_checked_) {
        return;
    }
    assets_version_checked_ = true;

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto& assets = Assets::GetInstance();

    if (!assets.partition_valid()) {
        ESP_LOGW(TAG, "Assets partition is disabled for board %s", BOARD_NAME);
        return;
    }
    
    Settings settings("assets", true);
    // Check if there is a new assets need to be downloaded
    std::string download_url = settings.GetString("download_url");

    if (!download_url.empty()) {
        settings.EraseKey("download_url");

        char message[256];
        snprintf(message, sizeof(message), Lang::Strings::FOUND_NEW_ASSETS, download_url.c_str());
        Alert(Lang::Strings::LOADING_ASSETS, message, "cloud_arrow_down", Lang::Sounds::OGG_UPGRADE);
        
        // Wait for the audio service to be idle for 3 seconds
        vTaskDelay(pdMS_TO_TICKS(3000));
        SetDeviceState(kDeviceStateUpgrading);
        board.SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
        display->SetChatMessage("system", Lang::Strings::PLEASE_WAIT);

        bool success = assets.Download(download_url, [this, display](int progress, size_t speed) -> void {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
            Schedule([display, message = std::string(buffer)]() {
                display->SetChatMessage("system", message.c_str());
            });
        });

        board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!success) {
            Alert(Lang::Strings::ERROR, Lang::Strings::DOWNLOAD_ASSETS_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
            vTaskDelay(pdMS_TO_TICKS(2000));
            SetDeviceState(kDeviceStateActivating);
            return;
        }
    }

    // Apply assets
    assets.Apply();
    display->SetChatMessage("system", "");
    display->SetEmotion("microchip_ai");
}

void Application::CheckNewVersion() {
    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // Initial retry delay in seconds

    auto& board = Board::GetInstance();
    while (true) {
        auto display = board.GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);

        esp_err_t err = ota_->CheckVersion();
        if (err != ESP_OK) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }

            char error_message[128];
            snprintf(error_message, sizeof(error_message), "code=%d, url=%s", err, ota_->GetCheckVersionUrl().c_str());
            char buffer[256];
            snprintf(buffer, sizeof(buffer), Lang::Strings::CHECK_NEW_VERSION_FAILED, retry_delay, error_message);
            Alert(Lang::Strings::ERROR, buffer, "cloud_slash", Lang::Sounds::OGG_EXCLAMATION);

            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", retry_delay, retry_count, MAX_RETRY);
            for (int i = 0; i < retry_delay; i++) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (GetDeviceState() == kDeviceStateIdle) {
                    break;
                }
            }
            retry_delay *= 2; // Double the retry delay
            continue;
        }
        retry_count = 0;
        retry_delay = 10; // Reset retry delay

        if (ota_->HasNewVersion()) {
            ESP_LOGI(TAG, "New version available: %s, but OTA is disabled", ota_->GetFirmwareVersion().c_str());
            // OTA 已禁用,不执行固件升级
        }

        // No new version, mark the current version as valid
        ota_->MarkCurrentVersionValid();
        if (!ota_->HasActivationCode() && !ota_->HasActivationChallenge()) {
            // Exit the loop if done checking new version
            break;
        }

        display->SetStatus(Lang::Strings::ACTIVATION);
        // Activation code is shown to the user and waiting for the user to input
        if (ota_->HasActivationCode()) {
            ShowActivationCode(ota_->GetActivationCode(), ota_->GetActivationMessage());
        }

        // This will block the loop until the activation is done or timeout
        for (int i = 0; i < 10; ++i) {
            ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
            esp_err_t err = ota_->Activate();
            if (err == ESP_OK) {
                break;
            } else if (err == ESP_ERR_TIMEOUT) {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } else {
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            if (GetDeviceState() == kDeviceStateIdle) {
                break;
            }
        }
    }
}

void Application::InitializeProtocol() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto codec = board.GetAudioCodec();

    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);

    if (ota_->HasMqttConfig()) {
        protocol_ = std::make_unique<MqttProtocol>();
    } else if (ota_->HasWebsocketConfig()) {
        protocol_ = std::make_unique<WebsocketProtocol>();
    } else {
        ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");
        protocol_ = std::make_unique<MqttProtocol>();
    }

    protocol_->OnConnected([this]() {
        DismissAlert();
    });

    protocol_->OnNetworkError([this](const std::string& message) {
        last_error_message_ = message;
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
    });
    
    protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
        if (GetDeviceState() == kDeviceStateSpeaking) {
            audio_service_.PushPacketToDecodeQueue(std::move(packet));
        }
    });
    
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
    });
    
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    SetDeviceState(kDeviceStateSpeaking);
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    if (GetDeviceState() == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(emotion)) {
                Schedule([display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "mcp") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            if (cJSON_IsObject(payload)) {
                McpServer::GetInstance().ParseMessage(payload);
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            auto command = cJSON_GetObjectItem(root, "command");
            if (cJSON_IsString(command)) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // Do a reboot if user requests a OTA update
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::OGG_VIBRATION);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
#if CONFIG_RECEIVE_CUSTOM_MESSAGE
        } else if (strcmp(type->valuestring, "custom") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            ESP_LOGI(TAG, "Received custom message: %s", cJSON_PrintUnformatted(root));
            if (cJSON_IsObject(payload)) {
                Schedule([this, display, payload_str = std::string(cJSON_PrintUnformatted(payload))]() {
                    display->SetChatMessage("system", payload_str.c_str());
                });
            } else {
                ESP_LOGW(TAG, "Invalid custom message format: missing payload");
            }
#endif
        } else {
            ESP_LOGW(TAG, "Unknown message type: %s", type->valuestring);
        }
    });
    
    protocol_->Start();
}

void Application::ShowActivationCode(const std::string& code, const std::string& message) {
    struct digit_sound {
        char digit;
        const std::string_view& sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{
        digit_sound{'0', Lang::Sounds::OGG_0},
        digit_sound{'1', Lang::Sounds::OGG_1}, 
        digit_sound{'2', Lang::Sounds::OGG_2},
        digit_sound{'3', Lang::Sounds::OGG_3},
        digit_sound{'4', Lang::Sounds::OGG_4},
        digit_sound{'5', Lang::Sounds::OGG_5},
        digit_sound{'6', Lang::Sounds::OGG_6},
        digit_sound{'7', Lang::Sounds::OGG_7},
        digit_sound{'8', Lang::Sounds::OGG_8},
        digit_sound{'9', Lang::Sounds::OGG_9}
    }};

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "link", Lang::Sounds::OGG_ACTIVATION);

    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            audio_service_.PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert [%s] %s: %s", emotion, status, message);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        audio_service_.PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (GetDeviceState() == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::ToggleChatState() {
    xEventGroupSetBits(event_group_, MAIN_EVENT_TOGGLE_CHAT);
}

void Application::StartListening() {
    xEventGroupSetBits(event_group_, MAIN_EVENT_START_LISTENING);
}

void Application::StopListening() {
    xEventGroupSetBits(event_group_, MAIN_EVENT_STOP_LISTENING);
}

void Application::LockDevice() {
    ESP_LOGI(TAG, "Locking device");
    if (SetDeviceState(kDeviceStateLocked)) {
        login_completion_claimed_.store(false, std::memory_order_release);
        Schedule([]() {
            ui_event_publish(UI_EVENT_LOGIN_REQUIRED);
        });
    }
}

void Application::OnLoginSuccess() {
    auto state = GetDeviceState();
    ESP_LOGI(TAG, "Completing login from state %s", DeviceStateMachine::GetStateName(state));
    if (state == kDeviceStateWifiConfiguring) {
        ESP_LOGI(TAG, "Login succeeded while WiFi is configuring; staying in config mode");
        return;
    }
    if (state == kDeviceStateIdle) {
        ESP_LOGI(TAG, "Ignoring login completion because device is already unlocked");
        return;
    }
    if (state != kDeviceStateLocked) {
        ESP_LOGW(TAG, "Ignoring login completion from unexpected state %s",
                 DeviceStateMachine::GetStateName(state));
        return;
    }

    bool expected = false;
    if (!login_completion_claimed_.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        ESP_LOGI(TAG, "Ignoring duplicate login completion");
        return;
    }

    if (!SetDeviceState(kDeviceStateIdle)) {
        ESP_LOGE(TAG, "Login accepted but device unlock failed from state %s",
                 DeviceStateMachine::GetStateName(state));
        login_completion_claimed_.store(false, std::memory_order_release);
        return;
    }
    ESP_LOGI(TAG, "Device unlocked after login");

    /*
     * Face login is a local security/scene trigger.  Do not wait for cloud
     * activation here; mqtt_send_scene() updates the local UI immediately and
     * queues the physical device sync until the smart-home MQTT link is ready.
     */
    mqtt_send_scene(IOT_SCENE_HOME);

    if (!network_ready_) {
        ESP_LOGW(TAG, "Login succeeded before network is ready; home scene queued");
        return;
    }

    if (!activation_done_ || !protocol_) {
        ESP_LOGI(TAG, "Login succeeded while protocol is still activating; smart home is already unlocked");
        return;
    }
}

void Application::HandleToggleChatEvent() {
    auto state = GetDeviceState();
    
    // 锁定状态下阻止所有交互
    if (state == kDeviceStateLocked) {
        ESP_LOGW(TAG, "Device is locked, ignoring toggle chat");
        return;
    }
    
    if (state == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    } else if (state == kDeviceStateWifiConfiguring) {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    } else if (state == kDeviceStateAudioTesting) {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (state == kDeviceStateIdle) {
        ListeningMode mode = GetDefaultListeningMode();
        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            // Schedule to let the state change be processed first (UI update)
            Schedule([this, mode]() {
                ContinueOpenAudioChannel(mode);
            });
            return;
        }
        SetListeningMode(mode);
    } else if (state == kDeviceStateSpeaking) {
        AbortSpeaking(kAbortReasonNone);
    } else if (state == kDeviceStateListening) {
        protocol_->CloseAudioChannel();
    }
}

void Application::ContinueOpenAudioChannel(ListeningMode mode) {
    // Check state again in case it was changed during scheduling
    if (GetDeviceState() != kDeviceStateConnecting) {
        return;
    }

    if (!protocol_->IsAudioChannelOpened()) {
        if (!protocol_->OpenAudioChannel()) {
            return;
        }
    }

    SetListeningMode(mode);
}

void Application::HandleStartListeningEvent() {
    auto state = GetDeviceState();
    
    if (state == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    } else if (state == kDeviceStateWifiConfiguring) {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (state == kDeviceStateIdle) {
        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            // Schedule to let the state change be processed first (UI update)
            Schedule([this]() {
                ContinueOpenAudioChannel(kListeningModeManualStop);
            });
            return;
        }
        SetListeningMode(kListeningModeManualStop);
    } else if (state == kDeviceStateSpeaking) {
        AbortSpeaking(kAbortReasonNone);
        SetListeningMode(kListeningModeManualStop);
    }
}

void Application::HandleStopListeningEvent() {
    auto state = GetDeviceState();
    
    if (state == kDeviceStateAudioTesting) {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    } else if (state == kDeviceStateListening) {
        if (protocol_) {
            protocol_->SendStopListening();
        }
        SetDeviceState(kDeviceStateIdle);
    }
}

void Application::HandleWakeWordDetectedEvent() {
    if (!protocol_) {
        return;
    }

    auto state = GetDeviceState();
    auto wake_word = audio_service_.GetLastWakeWord();
    ESP_LOGI(TAG, "Wake word detected: %s (state: %d)", wake_word.c_str(), (int)state);

    // 锁定状态下忽略唤醒词
    if (state == kDeviceStateLocked) {
        ESP_LOGW(TAG, "Device is locked, ignoring wake word");
        return;
    }

    if (state == kDeviceStateIdle) {
        audio_service_.EncodeWakeWord();
        auto wake_word = audio_service_.GetLastWakeWord();

        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            // Schedule to let the state change be processed first (UI update),
            // then continue with OpenAudioChannel which may block for ~1 second
            Schedule([this, wake_word]() {
                ContinueWakeWordInvoke(wake_word);
            });
            return;
        }
        // Channel already opened, continue directly
        ContinueWakeWordInvoke(wake_word);
    } else if (state == kDeviceStateSpeaking || state == kDeviceStateListening) {
        AbortSpeaking(kAbortReasonWakeWordDetected);
        // Clear send queue to avoid sending residues to server
        while (audio_service_.PopPacketFromSendQueue());

        if (state == kDeviceStateListening) {
            protocol_->SendStartListening(GetDefaultListeningMode());
            audio_service_.ResetDecoder();
            audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
            // Re-enable wake word detection as it was stopped by the detection itself
            audio_service_.EnableWakeWordDetection(true);
        } else {
            // Play popup sound and start listening again
            play_popup_on_listening_ = true;
            SetListeningMode(GetDefaultListeningMode());
        }
    } else if (state == kDeviceStateActivating) {
        // Restart the activation check if the wake word is detected during activation
        SetDeviceState(kDeviceStateIdle);
    }
}

void Application::ContinueWakeWordInvoke(const std::string& wake_word) {
    // Check state again in case it was changed during scheduling
    if (GetDeviceState() != kDeviceStateConnecting) {
        return;
    }

    if (!protocol_->IsAudioChannelOpened()) {
        if (!protocol_->OpenAudioChannel()) {
            audio_service_.EnableWakeWordDetection(true);
            return;
        }
    }

    ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
#if CONFIG_SEND_WAKE_WORD_DATA
    // Encode and send the wake word data to the server
    while (auto packet = audio_service_.PopWakeWordPacket()) {
        protocol_->SendAudio(std::move(packet));
    }
    // Set the chat state to wake word detected
    protocol_->SendWakeWordDetected(wake_word);

    // Set flag to play popup sound after state changes to listening
    play_popup_on_listening_ = true;
    SetListeningMode(GetDefaultListeningMode());
#else
    // Set flag to play popup sound after state changes to listening
    // (PlaySound here would be cleared by ResetDecoder in EnableVoiceProcessing)
    play_popup_on_listening_ = true;
    SetListeningMode(GetDefaultListeningMode());
#endif
}

void Application::HandleStateChangedEvent() {
    DeviceState new_state = state_machine_.GetState();
    clock_ticks_ = 0;

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();
    
    switch (new_state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            display->SetStatus(Lang::Strings::STANDBY);
            display->ClearChatMessages();  // Clear messages first
            display->SetEmotion("neutral"); // Then set emotion (wechat mode checks child count)
            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(true);
            break;
        case kDeviceStateLocked:
            // 锁定状态:禁用语音唤醒,显示锁定提示
            display->SetStatus(Lang::Strings::STANDBY);
            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(false);
            ESP_LOGI(TAG, "Device locked, waiting for login");
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            // Make sure the audio processor is running
            if (play_popup_on_listening_ || !audio_service_.IsAudioProcessorRunning()) {
                // For auto mode, wait for playback queue to be empty before enabling voice processing
                // This prevents audio truncation when STOP arrives late due to network jitter
                if (listening_mode_ == kListeningModeAutoStop) {
                    audio_service_.WaitForPlaybackQueueEmpty();
                }
                
                // Send the start listening command
                protocol_->SendStartListening(listening_mode_);
                audio_service_.EnableVoiceProcessing(true);
            }

#ifdef CONFIG_WAKE_WORD_DETECTION_IN_LISTENING
            // Enable wake word detection in listening mode (configured via Kconfig)
            audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
#else
            // Disable wake word detection in listening mode
            audio_service_.EnableWakeWordDetection(false);
#endif
            
            // Play popup sound after ResetDecoder (in EnableVoiceProcessing) has been called
            if (play_popup_on_listening_) {
                play_popup_on_listening_ = false;
                audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);

            if (listening_mode_ != kListeningModeRealtime) {
                audio_service_.EnableVoiceProcessing(false);
                // Only AFE wake word can be detected in speaking mode
                audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
            }
            audio_service_.ResetDecoder();
            break;
        case kDeviceStateWifiConfiguring:
            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(false);
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::Schedule(std::function<void()>&& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    if (protocol_) {
        protocol_->SendAbortSpeaking(reason);
    }
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

ListeningMode Application::GetDefaultListeningMode() const {
    return aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime;
}

void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    // Disconnect the audio channel
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        protocol_->CloseAudioChannel();
    }
    protocol_.reset();
    audio_service_.Stop();

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();

    std::string upgrade_url = url;
    std::string version_info = version.empty() ? "(Manual upgrade)" : version;

    // Close audio channel if it's open
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        ESP_LOGI(TAG, "Closing audio channel before firmware upgrade");
        protocol_->CloseAudioChannel();
    }
    ESP_LOGI(TAG, "Starting firmware upgrade from URL: %s", upgrade_url.c_str());

    Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download", Lang::Sounds::OGG_UPGRADE);
    vTaskDelay(pdMS_TO_TICKS(3000));

    SetDeviceState(kDeviceStateUpgrading);

    std::string message = std::string(Lang::Strings::NEW_VERSION) + version_info;
    display->SetChatMessage("system", message.c_str());

    board.SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
    audio_service_.Stop();
    vTaskDelay(pdMS_TO_TICKS(1000));

    bool upgrade_success = Ota::Upgrade(upgrade_url, [this, display](int progress, size_t speed) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
        Schedule([display, message = std::string(buffer)]() {
            display->SetChatMessage("system", message.c_str());
        });
    });

    if (!upgrade_success) {
        // Upgrade failed, restart audio service and continue running
        ESP_LOGE(TAG, "Firmware upgrade failed, restarting audio service and continuing operation...");
        audio_service_.Start(); // Restart audio service
        board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER); // Restore power save level
        Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        vTaskDelay(pdMS_TO_TICKS(3000));
        return false;
    } else {
        // Upgrade success, reboot immediately
        ESP_LOGI(TAG, "Firmware upgrade successful, rebooting...");
        display->SetChatMessage("system", "Upgrade successful, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Brief pause to show message
        Reboot();
        return true;
    }
}

void Application::WakeWordInvoke(const std::string& wake_word) {
    if (!protocol_) {
        return;
    }

    auto state = GetDeviceState();
    
    if (state == kDeviceStateIdle) {
        audio_service_.EncodeWakeWord();

        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            // Schedule to let the state change be processed first (UI update)
            Schedule([this, wake_word]() {
                ContinueWakeWordInvoke(wake_word);
            });
            return;
        }
        // Channel already opened, continue directly
        ContinueWakeWordInvoke(wake_word);
    } else if (state == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (state == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            }
        });
    }
}

bool Application::CanEnterSleepMode() {
    if (GetDeviceState() != kDeviceStateIdle) {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    if (!audio_service_.IsIdle()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::SendMcpMessage(const std::string& payload) {
    // Always schedule to run in main task for thread safety
    Schedule([this, payload = std::move(payload)]() {
        if (protocol_) {
            protocol_->SendMcpMessage(payload);
        }
    });
}

void Application::SetAecMode(AecMode mode) {
    aec_mode_ = mode;
    Schedule([this]() {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        switch (aec_mode_) {
        case kAecOff:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_OFF);
            break;
        case kAecOnServerSide:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        case kAecOnDeviceSide:
            audio_service_.EnableDeviceAec(true);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        }

        // If the AEC mode is changed, close the audio channel
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            protocol_->CloseAudioChannel();
        }
    });
}

void Application::PlaySound(const std::string_view& sound) {
    audio_service_.PlaySound(sound);
}

void Application::ResetProtocol() {
    Schedule([this]() {
        // Close audio channel if opened
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            protocol_->CloseAudioChannel();
        }
        // Reset protocol
        protocol_.reset();
        activation_done_ = false;
    });
}
