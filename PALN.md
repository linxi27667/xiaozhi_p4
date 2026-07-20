# ESP32-P4 小智 + LVGL 智能家居融合项目实施计划

> 文件名按当前需求写入 `PALN.md`。本文是后续真正编码实施的总计划，重点按 FreeRTOS 任务层架构拆分，保证小智聊天、智能家居 UI、IoT MQTT、TF 卡扩展和从机控制可以长期维护。

## 1. 项目目标

在当前小智 ESP32-P4 Function EV Board 主项目中，融合旧项目 `lvgl_demo_v9` 的 LVGL 智能家居控制面板，并废弃旧的独立 LVGL demo 工程。

最终效果：

- 设备启动后仍保留当前小智聊天能力。
- 屏幕左侧提供统一菜单栏。
- 左侧菜单可切换“小智聊天、总览、控制、网络、设置”。
- 小智聊天页保留主项目原有界面和业务更新逻辑。
- 智能家居页复用 `lvgl_demo_v9` 的页面设计和交互。
- 主机通过 MQTT 控制 `slave` 下的一楼、二楼、三楼从机。
- AI 语音控制和触摸 UI 控制共用同一套 IoT 控制服务和设备模型。
- TF 卡作为扩展存储，用于日志、历史状态、截图、资源缓存，不作为核心 UI 启动依赖。

## 2. 当前工程关键事实

主项目路径：

```text
E:\MCU\esp32\p4\xiaozhi-for-p4
```

目标板：

```text
CONFIG_IDF_TARGET_ESP32P4=y
CONFIG_BOARD_TYPE_ESP_P4_FUNCTION_EV_BOARD=y
```

主项目已有能力：

- ESP32-P4 双核 RISC-V 400MHz。
- 1024x600 MIPI LCD。
- GT911 触摸。
- PSRAM 200MHz。
- LVGL v9.4。
- `esp_lvgl_port` 显示锁和 LVGL handler。
- 小智聊天 UI：`main/display/lcd_display.*`。
- 小智主循环：`Application::Run()`，事件组驱动，主任务优先级运行时设为 10。
- 音频任务：`audio_input` 优先级 8，`audio_output` 优先级 4，`opus_codec` 优先级 2。
- P4 板级初始化中已有 `bsp_sdcard_mount()`，TF 卡挂载点 `/sdcard`。

旧智能家居 demo：

```text
lvgl_demo_v9
```

可迁移内容：

- `lvgl_demo_v9/main/ui`
- `lvgl_demo_v9/main/fonts`
- `lvgl_demo_v9/main/services/mqtt_iot_protocol.h`
- 必要的设备模型和 UI 事件分发逻辑

必须废弃内容：

- `lvgl_demo_v9/main/main.c`
- `lvgl_demo_v9/main/lvgl_adapter_init.*`
- `lvgl_demo_v9/main/task/task_ui.*`
- `lvgl_demo_v9/main/task/task_wifi.*`
- `lvgl_demo_v9/main/task/task_mqtt.*`

原因：主项目已经拥有唯一正确的 P4 BSP、LVGL port、LCD、触摸、网络和应用主循环。旧 demo 的独立初始化迁入会造成重复显示初始化、重复网络初始化和任务冲突。

## 3. 总体架构

### 3.1 分层结构

推荐新增或重组为以下架构：

```text
main/
├── application.*                  # 小智原有主循环，保持核心调度入口
├── display/
│   ├── lcd_display.*              # 小智原有 LCD 显示类，改造为 Shell 容器宿主
│   └── lvgl_display/
├── ui_shell/
│   ├── xz_ui_shell.h/.cc          # 统一左侧菜单 + 页面容器
│   ├── xz_chat_page.h/.cc         # 小智聊天页容器适配
│   └── xz_home_panel_page.h/.cc   # 智能家居页容器适配
├── smart_home/
│   ├── ui/                        # 从 lvgl_demo_v9/main/ui 迁移
│   ├── iot/                       # 主机 IoT MQTT 控制服务
│   ├── model/                     # 设备状态模型
│   ├── storage/                   # TF 卡日志/历史状态
│   └── tasks/                     # 任务层统一创建与管理
```

如果第一阶段想减少文件移动，也可以先把 `ui_shell` 放到 `main/display` 下，但长期建议独立目录，避免 `LcdDisplay` 越来越臃肿。

### 3.2 页面结构

只允许一个 LVGL display、一个 LVGL screen：

```text
lv_screen_active()
└── root_shell
    ├── sidebar_menu
    │   ├── 小智
    │   ├── 总览
    │   ├── 控制
    │   ├── 网络
    │   └── 设置
    └── page_host
        ├── chat_page
        └── smart_home_page
```

页面切换策略：

- `chat_page` 常驻，隐藏/显示切换。
- `smart_home_page` 首次进入时创建，后续隐藏/显示。
- 智能家居的子页面由迁移后的 `UI_Manager_Switch_Page()` 管理。
- 页面切换不删除小智聊天对象，避免 `SetChatMessage()`、`SetEmotion()` 在后台失效。

### 3.3 线程安全边界

LVGL 操作规则：

- 所有 LVGL 对象创建、删除、样式修改，必须在 `DisplayLockGuard` 或 `lvgl_port_lock()` 保护下执行。
- MQTT 回调、存储任务、模型任务不得直接操作 LVGL。
- 非 UI 任务要刷新界面时，只能投递 `SmartHomeUiEvent` 到 UI 队列，再由 UI 刷新路径处理。

小智主循环规则：

- 保留 `Application::Schedule()` 作为小智业务回到主任务的机制。
- 智能家居不把大量周期任务塞进 `Application::Schedule()`。
- 只有需要调用小智原有接口或 MCP 注册时，才进入 `Application` 主循环。

## 4. FreeRTOS 任务层设计

### 4.1 任务层总览

新增一个单独任务层，建议命名：

```text
main/smart_home/tasks/smart_home_tasks.h
main/smart_home/tasks/smart_home_tasks.cc
```

统一初始化入口：

```cpp
void SmartHomeTasks_Init();
void SmartHomeTasks_StartAfterNetworkReady();
void SmartHomeTasks_Stop();
```

任务创建原则：

- 主项目 `Application::Run()` 已经是优先级 10，新增任务不高于 7。
- 音频输入优先级 8，智能家居任务不能抢占音频实时链路。
- MQTT 控制和模型更新以事件驱动为主，避免 1 秒轮询式粗糙任务。
- 用队列传递结构体事件，用任务通知做轻量唤醒，用互斥锁保护共享模型。

### 4.2 新增任务清单

| 任务名 | 职责 | 优先级 | 栈建议 | 核心建议 | 启动时机 |
|---|---|---:|---:|---:|---|
| `sh_iot_mqtt` | IoT MQTT 连接、订阅、发布命令、接收从机消息 | 5 | 6144 | 不固定 | 网络连接后 |
| `sh_model` | 统一设备模型、在线状态、命令确认、状态合并 | 6 | 4096 | Core 1 | 系统启动后 |
| `sh_ui_evt` | 消费 UI 事件并安全刷新智能家居页面 | 4 | 4096 | Core 1 | LVGL Shell 创建后 |
| `sh_storage` | TF 卡日志、历史状态、配置快照异步写入 | 2 | 4096 | 不固定 | SD 挂载后 |
| `sh_diag` | 低频诊断、堆/栈水位、MQTT/从机健康状态 | 1 | 3072 | 不固定 | 调试配置开启时 |

不新增独立 `ui_task`。主项目已经有 `esp_lvgl_port` 负责 LVGL handler，旧 demo 的 `task_ui` 只做初始化和空循环，没有迁移价值。

### 4.3 队列和同步对象

建议新增：

```cpp
QueueHandle_t g_sh_cmd_queue;       // UI/AI -> IoT MQTT 命令
QueueHandle_t g_sh_rx_queue;        // MQTT callback -> model
QueueHandle_t g_sh_ui_queue;        // model -> UI
QueueHandle_t g_sh_storage_queue;   // model/UI -> TF 卡日志
SemaphoreHandle_t g_sh_model_mutex; // 保护设备模型快照
EventGroupHandle_t g_sh_events;     // 网络、MQTT、SD、模型脏标志
```

事件位建议：

```c
SH_EVENT_NETWORK_READY       (1 << 0)
SH_EVENT_MQTT_CONNECTED      (1 << 1)
SH_EVENT_SD_READY            (1 << 2)
SH_EVENT_MODEL_DIRTY         (1 << 3)
SH_EVENT_STOP_REQUEST        (1 << 4)
```

队列消息建议：

```cpp
enum class SmartHomeCommandType {
    Discover,
    SetLight,
    SetRelay,
    SetServo,
    AllLights,
    AllDevices,
};

struct SmartHomeCommand {
    SmartHomeCommandType type;
    uint8_t device_id;
    uint8_t index;
    uint8_t value;
    uint32_t request_id;
};

enum class SmartHomeRxType {
    Announce,
    Heartbeat,
    Response,
    Sensor,
    MqttState,
};

struct SmartHomeRxEvent {
    SmartHomeRxType type;
    uint8_t device_id;
    char mac[13];
    uint8_t payload[64];
    uint16_t payload_len;
    int64_t timestamp_ms;
};

enum class SmartHomeUiEventType {
    DeviceModelChanged,
    MqttStateChanged,
    SdStateChanged,
    Toast,
};

struct SmartHomeUiEvent {
    SmartHomeUiEventType type;
    uint8_t device_id;
    uint32_t flags;
};
```

队列长度建议：

```text
g_sh_cmd_queue       16
g_sh_rx_queue        32
g_sh_ui_queue        16
g_sh_storage_queue   32
```

溢出策略：

- 控制命令队列满：返回失败并 UI 提示“控制队列忙”。
- 接收队列满：丢弃低价值 heartbeat，保留 response/sensor。
- UI 队列满：合并成一次 `DeviceModelChanged`。
- 存储队列满：丢日志，不阻塞控制。

### 4.4 `sh_iot_mqtt` 任务

职责：

- 初始化独立 IoT MQTT client。
- 连接 Broker：`mqtt://8.134.167.240:1883`。
- 订阅：

```text
xiaozhi/iot/announce/+
xiaozhi/iot/heartbeat/+
xiaozhi/iot/resp/+
xiaozhi/iot/sensor/+
```

- 消费 `g_sh_cmd_queue`，发布：

```text
xiaozhi/iot/cmd/{device_id}
xiaozhi/iot/cmd/broadcast
```

- MQTT 回调不做业务处理，只将数据打包进 `g_sh_rx_queue`。

优先级：

```text
5
```

理由：控制响应需要及时，但不能高于音频输入和主小智状态机。

禁止事项：

- 不直接操作 LVGL。
- 不直接读写 TF 卡。
- 不直接修改 UI widget。
- 不调用阻塞很久的 DNS/连接逻辑在高优先级锁内执行。

### 4.5 `sh_model` 任务

职责：

- 消费 `g_sh_rx_queue`。
- 更新设备表、在线时间、状态、传感器值、命令确认。
- 处理离线判定。
- 向 `g_sh_ui_queue` 投递 UI 刷新事件。
- 向 `g_sh_storage_queue` 投递日志或历史状态。

设备表建议：

```cpp
struct SmartHomeDevice {
    uint8_t device_id;
    char name[24];
    char mac[13];
    bool online;
    uint32_t last_seen_ms;
    uint8_t lights[8];
    uint8_t relays[8];
    uint8_t servos[4];
    int sensor_smoke_mv;
    int sensor_rain_mv;
    bool fire_alarm;
    bool rain_alarm;
    bool help_alarm;
};
```

默认设备：

```text
1: 一楼设备
2: 二楼设备
3: 三楼设备
```

优先级：

```text
6
```

理由：模型是 UI 和控制一致性的中心，处理必须比 UI 绘制略及时，但仍低于音频输入。

离线策略：

- `heartbeat` 超过 30 秒未收到，标记离线。
- 离线事件只刷新 UI，不阻塞小智聊天。
- 设备重新 announce 或 heartbeat 后恢复在线。

### 4.6 `sh_ui_evt` 任务

职责：

- 消费 `g_sh_ui_queue`。
- 将模型变化合并为低频 UI 刷新。
- 通过 `DisplayLockGuard` 或专门 UI Shell API 刷新智能家居页面。

优先级：

```text
4
```

理由：UI 刷新可稍低，控制状态先进入模型，界面可延迟几十毫秒。

刷新节流：

- 收到大量状态变化时，50ms 内合并成一次刷新。
- 当前不在智能家居页面时，只更新模型和必要角标，不刷新复杂页面。
- 切回智能家居页时主动全量刷新一次。

注意：

该任务如果直接调用 LVGL，必须拿显示锁。如果后续发现与主项目显示锁有死锁风险，则改为通过 `Application::Schedule()` 投递到主任务执行 UI 刷新。

### 4.7 `sh_storage` 任务

职责：

- 检查 `/sdcard` 是否可用。
- 异步写入控制日志。
- 异步写入设备状态快照。
- 异步写入传感器历史。

优先级：

```text
2
```

理由：存储不能影响控制实时性。

文件建议：

```text
/sdcard/xiaozhi_home/control.log
/sdcard/xiaozhi_home/device_state.json
/sdcard/xiaozhi_home/sensor_history.csv
/sdcard/xiaozhi_home/screenshots/
```

写入策略：

- 日志追加。
- 状态快照 5 秒内最多写一次。
- SD 不存在时丢弃存储事件，只在设置页显示状态。
- 不在 UI 或 MQTT 任务中直接写卡。

### 4.8 `sh_diag` 任务

职责：

- 低频打印智能家居任务状态。
- 打印队列水位、堆余量、任务栈水位。
- 检查模型设备在线状态。

优先级：

```text
1
```

周期：

```text
10 秒
```

建议只在调试 Kconfig 打开：

```text
CONFIG_SMART_HOME_DIAG=y
```

## 5. 公共接口设计

### 5.1 SmartHome 服务入口

新增：

```cpp
class SmartHomeService {
public:
    static SmartHomeService& GetInstance();

    void Initialize();
    void OnNetworkReady();
    void OnNetworkLost();

    bool Discover();
    bool SetLight(uint8_t device_id, uint8_t index, bool on);
    bool SetRelay(uint8_t device_id, uint8_t index, bool on);
    bool SetServo(uint8_t device_id, uint8_t index, uint8_t step);
    bool AllLights(bool on);
    bool AllDevices(bool on);

    bool GetSnapshot(SmartHomeSnapshot* out);
};
```

调用方：

- UI 点击控制调用 `SmartHomeService`。
- MCP/AI 工具调用同一个 `SmartHomeService`。
- 设置页读取 `GetSnapshot()`。

### 5.2 UI Shell 接口

新增：

```cpp
enum class ShellPage {
    Chat,
    HomeData,
    HomeControl,
    HomeNetwork,
    HomeSettings,
};

class XiaozhiUiShell {
public:
    void Create(lv_obj_t* root, LcdDisplay* display);
    void SwitchTo(ShellPage page);
    lv_obj_t* ChatParent();
    lv_obj_t* SmartHomeParent();
    bool IsSmartHomeVisible() const;
    ShellPage CurrentPage() const;
};
```

`LcdDisplay::SetupUI()` 只负责创建 shell，然后把当前小智聊天 UI 挂到 `ChatParent()`。

### 5.3 旧 UI Manager 改造接口

旧接口：

```c
void UI_Manager_Init(lv_display_t *disp);
```

新接口：

```c
void UI_Manager_Init(lv_obj_t *parent);
void UI_Manager_Switch_Page(ui_page_id_t page);
void UI_Manager_Rebuild_Current(void);
void UI_Manager_Poll(void);
void UI_Manager_Refresh_Model(void);
```

关键要求：

- 不允许 `UI_Manager_Init()` 清空 `lv_screen_active()`。
- 不允许智能家居 UI 创建自己的全局 screen。
- 不再创建旧 demo 内部左侧栏，或将旧侧栏改成总 Shell 菜单的一部分。

## 6. 分阶段实施任务

### 阶段 0：基线验证

目标：确认当前主项目可独立构建运行。

任务：

- 执行 `idf.py build`。
- 记录当前 app 分区占用、heap 启动余量、PSRAM 余量。
- 启动硬件验证 LCD、触摸、小智聊天、音频、网络、TF 卡挂载。

验收：

- 当前项目无新增改动时构建通过。
- `ESP32P4FuncEV` 日志出现 SD 卡挂载结果。
- 小智聊天页可正常显示。

### 阶段 1：建立目录和任务层骨架

目标：先搭好维护边界，不迁移 UI 大量代码。

任务：

- 新增 `main/smart_home/tasks`。
- 新增 `SmartHomeTasks_Init()`。
- 新增队列、事件组、模型互斥锁。
- 新增空的 `SmartHomeService`。
- 在 `Application::Initialize()` 网络初始化后挂接 `SmartHomeService::Initialize()`。
- 在网络 connected callback 或 `HandleNetworkConnectedEvent()` 中调用 `SmartHomeService::OnNetworkReady()`。

验收：

- 构建通过。
- 启动日志能看到智能家居任务层初始化。
- 未连接 IoT MQTT 时不影响小智原有功能。

### 阶段 2：IoT MQTT 主机服务

目标：主项目可以独立连接 IoT Broker，并发现从机。

任务：

- 迁移 `mqtt_iot_protocol.h` 到 `main/smart_home/iot`。
- 实现 `sh_iot_mqtt`。
- 实现 command queue 发布。
- 实现 MQTT 回调转 `g_sh_rx_queue`。
- 实现 `Discover()` 广播。

验收：

- 三个从机上线后主机能收到 announce。
- 主机能收到 heartbeat。
- 不打开 UI 的情况下也能通过日志看到设备在线。
- MQTT 断线后自动重连，不重启系统。

### 阶段 3：设备模型与状态同步

目标：建立 UI/AI 共用设备模型。

任务：

- 实现 `SmartHomeDevice` 和 `SmartHomeSnapshot`。
- 实现 `sh_model`。
- 实现设备默认表：一楼、二楼、三楼。
- 实现 announce、heartbeat、response、sensor 解析。
- 实现离线检测。
- 实现 `GetSnapshot()`。

验收：

- 设备在线/离线状态准确。
- 控制响应能更新模型。
- 传感器数据能进入模型。
- `GetSnapshot()` 可被 UI 和 MCP 调用。

### 阶段 4：统一 UI Shell

目标：主界面出现统一左侧菜单，但智能家居页面先可为空。

任务：

- 新增 `XiaozhiUiShell`。
- 改造 `LcdDisplay::SetupUI()` 创建 Shell。
- 将小智聊天 UI 的父对象从 `screen` 调整为 `chat_page`。
- 左侧菜单实现 `Chat/HomeData/HomeControl/HomeNetwork/HomeSettings` 切换。

验收：

- 小智聊天功能不退化。
- 点击左侧菜单不会崩溃。
- 小智页隐藏后仍能接收 `SetChatMessage()` 和 `SetEmotion()` 更新。
- 切回小智页状态正确。

### 阶段 5：迁移智能家居 LVGL 页面

目标：旧 `lvgl_demo_v9` 智能家居 UI 在主项目右侧容器内运行。

任务：

- 迁移旧 UI 源码到 `main/smart_home/ui`。
- 改造 `UI_Manager_Init(lv_obj_t *parent)`。
- 移除旧 UI 内部侧栏或合并到总 Shell 菜单。
- 适配 include 路径和 CMake。
- 确认字体声明和 LVGL v9 API。

验收：

- `idf.py build` 通过。
- 总览、控制、网络、设置页面都能切换。
- 页面不清空小智 root screen。
- 页面布局适配 1024x600，左侧 72px 菜单后右侧内容不越界。

### 阶段 6：UI 与模型绑定

目标：智能家居 UI 显示真实设备状态并能发控制命令。

任务：

- 控制页按钮调用 `SmartHomeService`。
- 总览页显示设备在线状态、灯/继电器/舵机状态。
- 网络页显示 MQTT 连接状态、Broker、从机心跳。
- 设置页显示 TF 卡状态和调试信息。
- `sh_ui_evt` 根据模型变化刷新页面。

验收：

- UI 点击控制从机成功。
- 从机状态变化后 UI 自动刷新。
- 从机离线后 UI 显示离线。
- 快速点击不会卡住 LVGL。

### 阶段 7：MCP/AI 工具接入

目标：语音控制和触摸控制使用同一套 IoT 服务。

任务：

- 在 `McpServer` 或 P4 board 初始化路径注册智能家居工具。
- 工具调用 `SmartHomeService`，不直接发 MQTT。
- 支持：

```text
self.iot.discover
self.iot.set_light
self.iot.set_relay
self.iot.set_servo
self.iot.all_lights_on
self.iot.all_lights_off
self.iot.all_on
self.iot.all_off
self.iot.get_status
```

验收：

- 语音说“打开一楼灯”能控制对应从机。
- UI 与语音控制后的状态一致。
- 设备名别名可映射到一楼/二楼/三楼。

### 阶段 8：TF 卡扩展能力

目标：让 TF 卡可用但不成为核心依赖。

任务：

- 实现 `sh_storage`。
- 启动时检测 `/sdcard`。
- 写入控制日志。
- 写入设备状态快照。
- 设置页显示 SD 状态。

验收：

- 插卡时能写日志。
- 不插卡时系统正常启动。
- SD 写入失败不影响控制。

### 阶段 9：压力测试与优化

目标：保证长期运行稳定。

任务：

- 连续切换页面 100 次。
- 连续 UI 控制 100 次。
- 三个从机 heartbeat 连续运行 2 小时。
- 小智对话中切换智能家居页面。
- 打印栈水位、heap、PSRAM。

验收：

- 无 watchdog。
- 无 assert。
- 无明显 heap 泄漏。
- UI 不花屏。
- 音频对话不明显卡顿。

## 7. CMake 集成计划

在 `main/CMakeLists.txt` 中新增源文件分组，不建议使用过宽的 `GLOB_RECURSE` 覆盖整个旧 demo。

推荐显式或小范围 glob：

```cmake
file(GLOB SMART_HOME_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/core/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/pages/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/model/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/fonts/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smart_home/ui/services/*.c"
)

list(APPEND SOURCES
    "ui_shell/xz_ui_shell.cc"
    "ui_shell/xz_chat_page.cc"
    "ui_shell/xz_home_panel_page.cc"
    "smart_home/tasks/smart_home_tasks.cc"
    "smart_home/iot/smart_home_service.cc"
    "smart_home/iot/smart_home_mqtt.cc"
    "smart_home/model/smart_home_model.cc"
    "smart_home/storage/smart_home_storage.cc"
    ${SMART_HOME_UI_SOURCES}
)

list(APPEND INCLUDE_DIRS
    "ui_shell"
    "smart_home"
    "smart_home/tasks"
    "smart_home/iot"
    "smart_home/model"
    "smart_home/storage"
    "smart_home/ui"
    "smart_home/ui/core"
    "smart_home/ui/pages"
    "smart_home/ui/fonts"
    "smart_home/ui/services"
)
```

依赖方面主项目已有：

```text
mqtt
fatfs
esp_timer
esp_netif
esp_psram
```

若 MQTT 组件没有直接暴露给 main，则在 `idf_component_register(PRIV_REQUIRES ...)` 中确认保留 `mqtt`。

## 8. Kconfig 建议

新增配置：

```text
CONFIG_SMART_HOME_ENABLE=y
CONFIG_SMART_HOME_IOT_MQTT=y
CONFIG_SMART_HOME_STORAGE_SD=y
CONFIG_SMART_HOME_DIAG=n
CONFIG_SMART_HOME_BROKER_URI="mqtt://8.134.167.240"
CONFIG_SMART_HOME_BROKER_PORT=1883
CONFIG_SMART_HOME_DEFAULT_PAGE_CHAT=y
```

用途：

- 方便临时关闭智能家居功能定位问题。
- Broker 后续可配置。
- 诊断任务只在开发阶段启用。

## 9. 资源与字体计划

旧 demo 字体较多，不能无脑全部迁移。

第一阶段只保留实际页面使用的字体：

- 中文 14/16/20/24。
- 数字大字体如确实用于仪表盘再保留。
- 图标字体优先复用主项目 FontAwesome。

资源优先级：

1. 核心 UI 字体和图标：固件或 assets 分区。
2. 大图片、截图、历史数据：TF 卡。
3. 可缺省资源：TF 卡，不存在时降级。

验收：

- app 分区不超限。
- assets 分区不超限。
- 无重复定义字体符号。

## 10. 风险控制

### 10.1 LVGL 锁死

风险：UI 任务和主任务同时拿 LVGL 锁，造成死锁。

控制：

- 统一封装 `SmartHomeUi_PostRefresh()`。
- 不在持有 model mutex 时拿 LVGL lock。
- 不在持有 LVGL lock 时等待 MQTT/队列/存储。

### 10.2 音频被智能家居任务抢占

风险：控制任务优先级过高影响 `audio_input`。

控制：

- 新任务最高优先级为 6。
- MQTT 回调轻量化。
- 存储任务优先级 2。

### 10.3 MQTT 双系统混淆

风险：小智官方 MQTT 和 IoT MQTT 是两个系统。

控制：

- 小智对话协议继续由 `MqttProtocol/WebsocketProtocol` 管。
- 智能家居控制单独 `SmartHomeService` 管。
- 两者只在 MCP 工具层和 UI 层共享状态，不共享 client。

### 10.4 旧 demo 初始化误迁移

风险：迁入 `main.c/lvgl_adapter_init/task_wifi/task_mqtt`。

控制：

- 明确加入禁止迁移清单。
- 代码评审时检查是否出现 `esp_lv_adapter`、旧 `task_wifi_create()`、旧 `task_mqtt_create()`。

### 10.5 TF 卡卡顿

风险：FATFS 写入阻塞 UI 或控制。

控制：

- 只有 `sh_storage` 写卡。
- 队列满就丢日志。
- 状态快照限频。

## 11. 验收总清单

基础：

- `idf.py build` 通过。
- 启动无 crash。
- 小智聊天功能正常。
- 左侧菜单正常。

UI：

- 小智页、总览页、控制页、网络页、设置页都能切换。
- 页面切换 100 次无异常。
- 聊天页后台更新后切回正常。

IoT：

- 三个从机能 announce。
- heartbeat 能更新在线状态。
- UI 控制灯/继电器/舵机成功。
- AI 工具控制与 UI 状态一致。

FreeRTOS：

- 新增任务栈水位保留 30% 以上。
- 队列无长期满载。
- 无 watchdog。
- 音频任务无明显 starvation。

TF 卡：

- 插卡可写日志。
- 不插卡不影响核心功能。
- 设置页显示 SD 状态。

长期运行：

- 连续 2 小时无重启。
- heap/PSRAM 无持续下降。
- MQTT 断线重连成功。

## 12. 推荐实施顺序

严格按以下顺序推进：

1. 基线构建和硬件启动确认。
2. 新增任务层骨架和 `SmartHomeService` 空实现。
3. 实现 IoT MQTT 和设备模型，不碰 UI。
4. 实现统一 UI Shell，只保留小智聊天页。
5. 迁移智能家居 UI 页面到父容器。
6. 绑定 UI 和设备模型。
7. 接入 MCP/AI 工具。
8. 接入 TF 卡存储。
9. 压力测试和资源优化。

这个顺序的核心目的是每一步都可独立验证：先通信和模型，再做界面融合，最后做语音和存储扩展。这样出问题时能明确定位在任务层、UI 层、IoT 层还是资源层。

## 13. MCP 参考代码纳入计划

现有可参考 MCP 代码路径：

```text
E:\MCU\esp32\p4\mcp
```

目录中包含：

```text
iot_mcp_tool.h
iot_mcp_tool.cpp
alarm_mcp_tool.h
alarm_mcp_tool.cpp
custom_esp32s3_cam.cc
config.h
```

这些代码对本项目很有价值，但不能不加修改地整包复制。原因是旧 MCP 工具当前直接依赖：

```cpp
App_IOT_System_Init()
App_IOT_Set_GPIO()
App_IOT_Set_Light()
App_IOT_Set_Relay()
App_IOT_Set_Servo_ByIndex()
App_IOT_Broadcast_All_Off()
App_IOT_Broadcast_All_On()
App_IOT_Get_All_Status()
```

而本项目融合后的正确依赖应是：

```cpp
SmartHomeService::GetInstance()
```

所以 MCP 代码采用“保留工具定义和语义，替换底层调用”的迁移方式。

### 13.1 可直接复用的内容

可以直接参考或复制后改名的内容：

- `custom_esp32s3_cam.cc` 中 `McpServer::GetInstance().AddTool(...)` 的工具注册写法。
- `self.iot.set_light` 的设备/灯映射提示词。
- `self.iot.set_relay` 的继电器工具参数。
- `self.iot.set_servo_by_index` 的舵机角度工具参数。
- `self.iot.clothes_rack` 的晾衣架语义映射。
- `self.iot.all_off`、`self.iot.all_on`、`self.iot.all_lights_off`、`self.iot.all_lights_on` 的广播工具语义。
- `self.iot.get_status`、`self.iot.get_sensors`、`self.iot.get_history` 的 JSON 返回模式。
- `self.iot.discover` 的发现工具结构。
- `alarm_mcp_tool.*` 的闹钟数据结构和 MCP 工具思路。

不建议直接复用的内容：

- `custom_esp32s3_cam.cc` 整个板级类。
- 摄像头、YOLO、MPU6050、蜂鸣器相关工具。
- 旧 `StartNetwork()` 里等待 Wi-Fi 后启动 IoT 的写法。
- `iot_mcp_tool.cpp` 中直接调用 `App_IOT_*` 的底层实现。
- `Alarm_Mcp_Init()` 中每次初始化都清空 NVS 的行为。

### 13.2 新 MCP 模块位置

建议在主项目新增：

```text
main/smart_home/mcp/smart_home_mcp_tool.h
main/smart_home/mcp/smart_home_mcp_tool.cc
main/smart_home/mcp/alarm_mcp_tool.h
main/smart_home/mcp/alarm_mcp_tool.cc
```

如果第一阶段只做 IoT，不做闹钟，则先只新增：

```text
main/smart_home/mcp/smart_home_mcp_tool.h
main/smart_home/mcp/smart_home_mcp_tool.cc
```

### 13.3 MCP 初始化入口

不要把 MCP 注册写进 P4 板级构造函数里。推荐在 `Application::Initialize()` 中 `mcp_server.AddCommonTools()` 和 `mcp_server.AddUserOnlyTools()` 之后调用：

```cpp
#if CONFIG_SMART_HOME_ENABLE
SmartHomeMcp_RegisterTools();
#endif
```

这样 MCP 工具注册归属于应用层，而不是某个旧 S3-CAM board 类。

如果后续希望 P4 board 专属注册，也可以放入：

```text
main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc
```

但不推荐第一版这样做，原因是智能家居能力是应用功能，不是 LCD/音频板级能力。

### 13.4 新 MCP 工具清单

第一版必须注册：

```text
self.iot.discover
self.iot.get_status
self.iot.get_sensors
self.iot.get_history
self.iot.set_light
self.iot.set_relay
self.iot.set_servo_by_index
self.iot.clothes_rack
self.iot.all_off
self.iot.all_on
self.iot.all_lights_off
self.iot.all_lights_on
```

第二版可选注册：

```text
self.scene.homing
self.scene.good_night
self.scene.leave_home
self.scene.emergency
self.scene.rain_collect_clothes
self.scene.ventilate
```

闹钟类工具单独评估，不和智能家居第一阶段混在一起：

```text
self.alarm.add_timer
self.alarm.add_timer_seconds
self.alarm.add_alarm
self.alarm.get_list
self.alarm.dismiss
self.alarm.snooze
```

### 13.5 MCP 工具到底层服务的映射

旧工具调用：

```cpp
App_IOT_Set_Light(device.c_str(), index, value);
```

新工具应调用：

```cpp
SmartHomeService::GetInstance().SetLightByName(device.c_str(), index, value);
```

建议给 `SmartHomeService` 补充按名称控制 API：

```cpp
bool SetLightByName(const char* device_name, uint8_t index, bool on);
bool SetRelayByName(const char* device_name, uint8_t index, bool on);
bool SetServoByName(const char* device_name, uint8_t index, uint8_t angle);
bool ClothesRackByName(const char* device_name, const char* action);
bool BroadcastAll(bool on);
bool BroadcastLights(bool on);
void GetStatusJson(char* buffer, size_t len);
void GetSensorsJson(char* buffer, size_t len);
void GetHistoryJson(char* buffer, size_t len);
```

名称解析规则由 `SmartHomeService` 统一维护，不能散落在 MCP lambda、UI 页面和模型任务中。

默认别名表：

```text
一楼设备 / 一楼 / 1F / 大厅 / 大门        -> device_id=1
二楼设备 / 二楼 / 2F / 主卧 / 客厅 / 风扇  -> device_id=2
三楼设备 / 三楼 / 3F / 阳台 / 天窗        -> device_id=3
```

### 13.6 MCP 工具参数保留方案

保留旧项目中已经写得比较完整的提示词映射。

灯光工具：

```text
self.iot.set_light(device, index, value)
```

映射：

```text
一楼设备: index0=大厅灯
二楼设备: index0=主卧灯, index1=客厅灯, index2=厕所灯
三楼设备: index0=阳台灯
```

继电器工具：

```text
self.iot.set_relay(device, index, value)
```

映射：

```text
二楼设备: index0=风扇
```

舵机工具：

```text
self.iot.set_servo_by_index(device, index, angle)
```

映射：

```text
一楼设备: index0=大门, 0=关闭, 135=打开
二楼设备: index0=晾衣架, 0=收衣服, 180=晾衣服
三楼设备: index0=左天窗, index1=右天窗, index2=晾衣架
```

晾衣架工具：

```text
self.iot.clothes_rack(device, action)
```

参数：

```text
action=hang    -> 180
action=collect -> 0
```

广播工具：

```text
self.iot.all_off
self.iot.all_on
self.iot.all_lights_off
self.iot.all_lights_on
```

### 13.7 MCP 返回 JSON 统一规范

所有控制工具返回：

```json
{
  "success": true,
  "device": "二楼设备",
  "type": "light",
  "index": 0,
  "value": true,
  "request_id": 123,
  "message": "灯已打开"
}
```

失败时：

```json
{
  "success": false,
  "error": "device_not_found",
  "message": "未找到设备"
}
```

状态工具返回：

```json
{
  "mqtt_connected": true,
  "devices": [
    {
      "id": 1,
      "name": "一楼设备",
      "online": true,
      "lights": [true],
      "relays": [],
      "servos": [180]
    }
  ]
}
```

传感器工具返回：

```json
{
  "devices": [
    {
      "name": "三楼设备",
      "smoke_mv": 1200,
      "rain_mv": 2600,
      "fire_alarm": false,
      "rain_alarm": false,
      "help_alarm": false
    }
  ]
}
```

历史工具返回最近 50 条事件，来源优先为内存 ring buffer，TF 卡可用时再扩展为持久历史。

### 13.8 MCP 与 FreeRTOS 任务层关系

MCP 工具运行时只做三件事：

1. 解析参数。
2. 调用 `SmartHomeService`。
3. 构建 JSON 返回。

MCP 工具不做：

- 不直接发布 MQTT。
- 不直接改 `SmartHomeDevice` 模型。
- 不直接写 TF 卡。
- 不直接刷新 LVGL。
- 不 `vTaskDelay()` 等待长时间响应。

`self.iot.discover` 旧代码会 `Iot_Mcp_Wait_For_Response(3000)` 阻塞等待 3 秒。新实现不建议阻塞 3 秒，推荐：

- 发送 Discover 命令。
- 立即返回当前已知设备数量和 `discover_started=true`。
- UI 和状态工具通过模型后续显示发现结果。

如果确实需要等待，最多等待 500ms，并且不能在持有任何模型锁时等待。

### 13.9 闹钟 MCP 的处理策略

`alarm_mcp_tool.*` 可以作为第二阶段功能参考，但需要修正后再迁入。

当前参考代码问题：

- `Alarm_Mcp_Init()` 会清空 NVS 中旧闹钟，不适合作为产品行为。
- 全局 `g_alarm_manager` 没有互斥锁，MCP 调用和处理任务并发时可能竞态。
- `Alarm_Mcp_Process()` 需要周期任务，但原项目任务代码不在独立模块中完整抽出。

如果迁入，建议新增任务：

```text
sh_alarm
```

任务参数：

```text
priority=2
stack=4096
period=1000ms
```

并增加：

```cpp
SemaphoreHandle_t g_alarm_mutex;
```

闹钟存储：

- 第一版用 NVS。
- 不在初始化时清空。
- 后续可选同步到 TF 卡。

闹钟触发：

- 通过 `Application::Schedule()` 调用 `Alert()`。
- 蜂鸣器相关代码不直接复制，P4 当前项目未确认蜂鸣器硬件。

### 13.10 文件复制清单

第一阶段可复制为模板并改名：

```text
E:\MCU\esp32\p4\mcp\iot_mcp_tool.h
  -> main/smart_home/mcp/smart_home_mcp_tool.h

E:\MCU\esp32\p4\mcp\iot_mcp_tool.cpp
  -> main/smart_home/mcp/smart_home_mcp_tool.cc
```

复制后必须修改：

```text
#include "iot_controller.h"
  改为
#include "smart_home_service.h"

所有 App_IOT_* 调用
  改为 SmartHomeService::* 调用

Iot_Mcp_* 函数名前缀
  改为 SmartHomeMcp_* 或放入 C++ namespace smart_home
```

从 `custom_esp32s3_cam.cc` 中只复制工具注册代码块，不复制板级类：

```text
InitializeTools() 内 self.iot.* 的 AddTool 片段
```

目标位置：

```text
main/smart_home/mcp/smart_home_mcp_tool.cc
```

封装为：

```cpp
void SmartHomeMcp_RegisterTools();
```

第二阶段可复制为模板并修正：

```text
alarm_mcp_tool.h/.cpp
```

目标位置：

```text
main/smart_home/mcp/alarm_mcp_tool.h
main/smart_home/mcp/alarm_mcp_tool.cc
```

### 13.11 CMake 追加计划

在 `main/CMakeLists.txt` 中追加：

```cmake
list(APPEND SOURCES
    "smart_home/mcp/smart_home_mcp_tool.cc"
)

list(APPEND INCLUDE_DIRS
    "smart_home/mcp"
)
```

如果启用闹钟：

```cmake
list(APPEND SOURCES
    "smart_home/mcp/alarm_mcp_tool.cc"
)
```

### 13.12 MCP 验收标准

构建验收：

- `idf.py build` 通过。
- 无 `App_IOT_*` 未定义符号。
- 无旧 `custom_esp32s3_cam` 板级依赖。

工具注册验收：

- 启动日志显示 `SmartHome MCP tools registered`。
- MCP 工具列表包含 `self.iot.*`。

功能验收：

- “打开大厅灯”调用 `self.iot.set_light(device=一楼设备,index=0,value=true)`。
- “关闭风扇”调用 `self.iot.set_relay(device=二楼设备,index=0,value=false)`。
- “打开一楼大门”调用 `self.iot.set_servo_by_index(device=一楼设备,index=0,angle=135)`。
- “晾衣服”调用 `self.iot.clothes_rack(device=二楼设备,action=hang)`。
- “关闭所有灯”调用 `self.iot.all_lights_off`。
- “查看设备状态”调用 `self.iot.get_status`。
- “查看传感器”调用 `self.iot.get_sensors`。

一致性验收：

- MCP 控制后 UI 状态同步刷新。
- UI 控制后 `self.iot.get_status` 返回一致状态。
- 从机离线时 MCP 返回明确错误，不假报成功。
