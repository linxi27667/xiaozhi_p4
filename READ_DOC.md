# ESP32-P4 小智项目融合 LVGL 智能家居控制面板评估文档

## 1. 结论

当前项目把 `lvgl_demo_v9` 中的 LVGL 智能家居控制面板融合进小智主工程是可行的。

推荐方案不是把 `lvgl_demo_v9` 整个工程拷进来运行，而是把它拆成“可复用 UI 层”和“必须废弃的独立工程层”：

- 保留当前小智项目原有的 ESP32-P4 板级初始化、LCD/触摸初始化、音频链路、Wi-Fi Remote/ESP-Hosted 网络链路、小智聊天界面和 `LcdDisplay` 显示抽象。
- 迁移 `lvgl_demo_v9/main/ui` 下的智能家居页面、样式、事件、模型和字体资源。
- 废弃 `lvgl_demo_v9/main/main.c`、`lvgl_adapter_init.*`、`task_wifi.*`、`task_mqtt.*` 这些独立 demo 初始化和任务入口。
- 在主项目中新增统一 LVGL Shell：左侧菜单栏切换“小智聊天”和“智能家居控制”，右侧内容区加载对应页面。
- 小智聊天页保留本项目原有界面能力，不重做小智聊天业务逻辑。
- TF 卡可以使用，但建议作为扩展存储，不作为核心 UI 启动依赖。

ESP32-P4 Function EV Board 的性能足够完成该目标。项目当前已启用 ESP32-P4、16MB Flash、PSRAM、LVGL v9.4、1024x600 MIPI LCD、触摸、SD 卡挂载能力，硬件方向匹配这个融合目标。

## 2. 当前项目事实

### 2.1 主项目状态

项目根目录：

```text
E:\MCU\esp32\p4\xiaozhi-for-p4
```

当前主项目配置中已经选择：

```text
CONFIG_IDF_TARGET_ESP32P4=y
CONFIG_BOARD_TYPE_ESP_P4_FUNCTION_EV_BOARD=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_LV_USE_SNAPSHOT=y
```

分区表使用：

```text
partitions/v2/16m.csv
```

当前分区布局：

```text
nvs      0x4000
otadata  0x2000
phy_init 0x1000
ota_0    0x3f0000
ota_1    0x3f0000
assets   8M
```

主项目已经依赖：

```text
lvgl/lvgl: ~9.4.0
esp_lvgl_port: ~2.7.0
espressif/esp32_p4_function_ev_board
espressif/esp_hosted
espressif/esp_wifi_remote
fatfs
```

这说明主项目已经具备成熟的 P4 板级、显示、触摸、网络和资源体系。

### 2.2 主项目原有小智 LVGL 界面

主项目的 LVGL 显示抽象集中在：

```text
main/display/lvgl_display/
main/display/lcd_display.h
main/display/lcd_display.cc
```

核心类：

```cpp
LvglDisplay
LcdDisplay
MipiLcdDisplay
```

小智聊天界面主要通过这些接口更新：

```cpp
SetupUI()
SetChatMessage()
SetEmotion()
SetStatus()
ShowNotification()
UpdateStatusBar()
ClearChatMessages()
```

应用层、音频层和协议层已经依赖这些接口，所以融合智能家居 UI 时不应破坏这些接口。

### 2.3 P4 Function EV Board 初始化

板级文件：

```text
main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc
```

它已经负责：

- 初始化 BSP I2C。
- 初始化 MIPI LCD。
- 创建 `MipiLcdDisplay(handles.io, handles.panel, 1024, 600, ...)`。
- 初始化触摸。
- 初始化 SD 卡：`bsp_sdcard_mount()`。
- 初始化摄像头。
- 初始化字体支持。
- 初始化背光。

这部分应继续作为唯一硬件初始化入口。

### 2.4 旧智能家居 LVGL demo 状态

旧项目路径：

```text
E:\MCU\esp32\p4\xiaozhi-for-p4\lvgl_demo_v9
```

旧项目也使用 LVGL v9.4，方向与主项目一致。

旧 demo 的 UI 管理器：

```text
lvgl_demo_v9/main/ui/ui_manager.h
lvgl_demo_v9/main/ui/ui_manager.c
```

当前 UI 页面：

```text
UI_PAGE_DATA
UI_PAGE_CTRL
UI_PAGE_NET
UI_PAGE_SET
```

旧 UI 已经有左侧菜单栏和内容区，默认 1024x600，左侧栏约 72px，页面包括：

```text
lvgl_demo_v9/main/ui/pages/page_data.*
lvgl_demo_v9/main/ui/pages/page_ctrl.*
lvgl_demo_v9/main/ui/pages/page_net.*
lvgl_demo_v9/main/ui/pages/page_set.*
```

旧 demo 独立入口：

```text
lvgl_demo_v9/main/main.c
```

它会独立做：

- SPIFFS 挂载。
- LVGL adapter 初始化。
- BSP display 初始化。
- Wi-Fi 任务创建。
- MQTT 任务创建。

这些不能直接迁移到主项目，否则会出现两套显示初始化、两套网络初始化、两套 MQTT/状态模型，导致冲突。

### 2.5 从机项目状态

从机路径：

```text
E:\MCU\esp32\p4\xiaozhi-for-p4\slave
```

存在三个从机工程：

```text
slave/xiaozhi_slave_Firstfloor
slave/xiaozhi_slave_Secondfloor
slave/xiaozhi_slave_Thirdfloor
```

从机当前采用 Wi-Fi + MQTT 控制方式，核心 Topic 约定：

```text
xiaozhi/iot/cmd/{device_id}
xiaozhi/iot/cmd/broadcast
xiaozhi/iot/resp/{mac_hex}
xiaozhi/iot/heartbeat/{mac_hex}
xiaozhi/iot/announce/{mac_hex}
xiaozhi/iot/sensor/{mac_hex}
```

旧 demo 中的 `mqtt_iot_protocol.h` 与从机协议方向一致，可作为主项目 UI 控制和 AI 控制的统一协议基础。

## 3. 推荐总体架构

### 3.1 融合后的目标结构

推荐在主项目中形成如下结构：

```text
main/
├── display/
│   ├── lcd_display.*
│   └── lvgl_display/
├── ui_shell/
│   ├── xz_ui_shell.h/.cc
│   ├── xz_chat_page.h/.cc
│   └── xz_home_panel_page.h/.cc
├── smart_home/
│   ├── ui/
│   │   ├── ui_manager.*
│   │   ├── ui_theme.*
│   │   ├── ui_styles.*
│   │   ├── ui_events.*
│   │   ├── pages/
│   │   ├── model/
│   │   ├── fonts/
│   │   └── services/
│   └── iot/
│       ├── iot_controller.*
│       ├── mqtt_iot_protocol.h
│       └── iot_device_model.*
```

也可以不新增 `ui_shell` 文件夹，而是先把 Shell 集成在 `LcdDisplay` 内部。但从长期维护看，建议单独抽出 Shell 层。

### 3.2 页面关系

融合后 LVGL 只有一个屏幕根对象：

```text
lv_screen_active()
```

根对象下创建统一 Shell：

```text
root_screen
├── sidebar
│   ├── menu_chat
│   └── menu_smart_home
└── content_area
    ├── chat_page_container
    └── smart_home_page_container
```

切换页面时建议先采用显示/隐藏策略：

- 聊天页对象常驻，避免小智状态栏、字幕、表情对象被反复删除。
- 智能家居页可以首次进入时创建，后续隐藏/显示；如果内存压力大，再改为离开页面时清理。

### 3.3 小智聊天页处理

小智聊天界面必须保留原接口：

```cpp
SetChatMessage()
SetEmotion()
SetStatus()
ShowNotification()
UpdateStatusBar()
ClearChatMessages()
```

改造原则：

- 不改变应用层调用方式。
- 不把小智聊天 UI 改写成旧 demo 的页面。
- 只把小智当前创建在 `screen` 上的控件，改为创建在 `chat_page_container` 内。
- 页面不可见时仍允许 `SetChatMessage()` 更新内部对象，用户切回聊天页时应看到最新状态。

### 3.4 智能家居页处理

旧 `UI_Manager_Init(lv_display_t *disp)` 当前会直接操作 `lv_screen_active()`，需要改为：

```c
void UI_Manager_Init(lv_obj_t *parent);
```

旧逻辑：

```c
lv_obj_t *scr = lv_screen_active();
lv_obj_remove_style_all(scr);
create_sidebar(scr);
create_content_area(scr);
```

应调整为：

```c
lv_obj_t *scr = parent;
lv_obj_remove_style_all(scr);
create_sidebar(scr);
create_content_area(scr);
```

但要注意：旧智能家居 UI 自己也有左侧菜单栏。融合后有两种选择：

1. 保留旧智能家居内部四页菜单栏，只在外层菜单选择“小智/家居”。
2. 合并为一个总菜单栏，左侧同时列出“小智、总览、控制、网络、设置”。

推荐第 2 种：一个总菜单栏。这样符合用户“通过左侧菜单栏切换”的需求，也避免出现“双侧栏”。最终菜单建议：

```text
小智
总览
控制
网络
设置
```

点击“小智”显示小智聊天页；点击其余项显示智能家居容器并切到对应子页面。

## 4. 实施步骤

### 阶段 1：文档和边界确认

目标：

- 明确 `lvgl_demo_v9` 作为独立工程废弃。
- 明确主项目硬件初始化唯一入口仍是 `esp-p4-function-ev-board.cc`。
- 明确主项目 `LcdDisplay` 接口不可破坏。

输出：

- 本文档。
- 后续实现时按本文档执行。

### 阶段 2：迁移智能家居 UI 源码

迁移内容：

```text
lvgl_demo_v9/main/ui/ui_manager.*
lvgl_demo_v9/main/ui/ui_theme.*
lvgl_demo_v9/main/ui/ui_anim.*
lvgl_demo_v9/main/ui/core/*
lvgl_demo_v9/main/ui/pages/*
lvgl_demo_v9/main/ui/model/*
lvgl_demo_v9/main/ui/fonts/*
lvgl_demo_v9/main/ui/services/*
lvgl_demo_v9/main/fonts/*
```

不迁移内容：

```text
lvgl_demo_v9/main/main.c
lvgl_demo_v9/main/lvgl_adapter_init.*
lvgl_demo_v9/main/task/task_wifi.*
lvgl_demo_v9/main/task/task_mqtt.*
```

原因：

- 主项目已经有 LVGL port 和 BSP display 初始化。
- 主项目已经有网络体系。
- 独立 demo 的入口会与主项目冲突。

### 阶段 3：建立统一菜单和页面容器

改造 `LcdDisplay::SetupUI()`：

- 创建根 Shell 容器。
- 创建左侧菜单栏。
- 创建右侧内容区。
- 创建聊天页容器。
- 创建智能家居页容器。
- 默认显示“小智聊天页”或“总览页”均可，推荐默认显示“小智聊天页”，符合小智设备启动预期。

推荐页面枚举：

```cpp
enum class MainUiPage {
    Chat,
    HomeData,
    HomeControl,
    HomeNetwork,
    HomeSettings,
};
```

切换规则：

- `Chat`：显示聊天页容器，隐藏智能家居页容器。
- `HomeData/HomeControl/HomeNetwork/HomeSettings`：隐藏聊天页容器，显示智能家居页容器，并调用智能家居页面切换 API。

### 阶段 4：调整小智聊天 UI 父对象

把原来直接挂在 `lv_screen_active()` 上的对象改到聊天页容器中。

重点对象：

```cpp
container_
emoji_box_
preview_image_
top_bar_
status_bar_
bottom_bar_
low_battery_popup_
emoji_label_
emoji_image_
chat_message_label_
```

弹窗类对象可以仍挂在 `lv_screen_active()`，但要设置高层级，避免被页面容器裁剪。低电量弹窗、通知栏建议继续作为全局对象。

### 阶段 5：接入智能家居 UI

将旧 UI 管理器改成父容器版本：

```c
void UI_Manager_Init(lv_obj_t *parent);
void UI_Manager_Switch_Page(ui_page_id_t page);
void UI_Manager_Rebuild_Current(void);
void UI_Manager_Poll(void);
```

在 Shell 初始化时：

```cpp
UI_Manager_Init(smart_home_page_container_);
```

菜单点击时：

```cpp
UI_Manager_Switch_Page(UI_PAGE_DATA);
UI_Manager_Switch_Page(UI_PAGE_CTRL);
UI_Manager_Switch_Page(UI_PAGE_NET);
UI_Manager_Switch_Page(UI_PAGE_SET);
```

### 阶段 6：统一 IoT 数据模型

应新增主项目 IoT 控制服务，而不是继续使用旧 demo 的 `task_mqtt`。

推荐职责：

- 连接 IoT MQTT Broker。
- 订阅从机 announce、heartbeat、resp、sensor Topic。
- 维护设备在线状态、设备名称、楼层、灯/继电器/舵机/传感器状态。
- 给 UI 提供只读查询和控制 API。
- 给 AI MCP 工具提供同一套控制 API。

推荐 API：

```c
void IotController_Init(void);
bool IotController_IsReady(void);
void IotController_Discover(void);
void IotController_SetLight(uint8_t device_id, uint8_t index, bool on);
void IotController_SetRelay(uint8_t device_id, uint8_t index, bool on);
void IotController_SetServo(uint8_t device_id, uint8_t index, uint8_t step);
void IotController_AllLights(bool on);
void IotController_AllDevices(bool on);
```

UI 事件不直接发布 MQTT，应调用 IoT Controller API。MQTT 回调也不直接改 LVGL 对象，应更新模型并投递 UI 事件。

## 5. 性能评估

### 5.1 CPU

ESP32-P4 是双核 RISC-V，主频 400MHz，定位是多媒体和高性能 HMI。当前需求包括：

- LVGL 1024x600 UI。
- 小智聊天 UI。
- 智能家居控制 UI。
- MQTT 收发。
- 音频采集/播放。
- 唤醒词/语音链路。
- 触摸输入。

这些任务对 P4 来说可承载。

主要 CPU 风险不是“跑不动”，而是：

- LVGL 页面频繁整页重建。
- MQTT 回调中做复杂 UI 操作。
- 大量动画、阴影、透明叠加同时刷新。
- 字体和图片资源加载方式不合理。

控制策略：

- LVGL 刷新周期保持当前配置 `CONFIG_LV_DEF_REFR_PERIOD=33`，目标约 30 FPS。
- 页面切换使用隐藏/显示或局部重建，不频繁清空整个屏幕。
- UI 控制事件走队列或事件分发，不在 MQTT 回调直接操作 LVGL。
- 避免大量全屏透明和复杂阴影。

### 5.2 内存

当前项目启用了 PSRAM：

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536
```

这对 1024x600 LVGL UI 很关键。

内存可行性判断：

- 小智原有 LVGL UI 已经能在 P4 项目中运行。
- 旧智能家居面板是常规 LVGL 页面、按钮、标签、卡片和字体，不是视频级负载。
- P4 + PSRAM 足够承载两个 UI 功能集。

主要内存风险：

- 聊天记录气泡数量无限增长。
- 智能家居页面全部常驻且包含大字体/大图片。
- 字体重复编译多份。
- 旧 demo 字体和主项目字体重复占用 Flash/内存。

控制策略：

- 聊天记录继续保持数量上限，当前 P4 下已有 `MAX_MESSAGES 40` 逻辑。
- 智能家居页面优先只常驻模型，不强制所有页面对象永久存在。
- 字体统一管理，优先复用主项目 `xiaozhi-fonts` 或合并旧 demo 字体。
- 大资源放 assets 分区或 TF 卡，不直接堆在 RAM。

### 5.3 Flash

当前 16MB Flash 分区中：

- 两个 OTA app 分区各约 0x3f0000，约 4MB。
- assets 分区 8MB。

融合后 app 体积会上升，主要来自：

- 智能家居 UI C 文件。
- 中文字体。
- 图标字体。
- IoT 控制代码。

评估：

- UI 代码本身体积不会很大。
- 字体是主要 Flash 消耗点。
- 当前 assets 8MB 可承载较多字体/表情/图片资源。

建议：

- 字体优先放 assets 或复用现有字体组件。
- 不要同时保留旧 demo 的多套字号中文字体，先保留 UI 实际用到的字号。
- 编译后检查 app 分区剩余空间，必要时调整分区或减少字体。

## 6. TF 卡用途评估

当前 P4 板级代码已经有：

```cpp
InitializeSdCard()
{
    esp_err_t ret = bsp_sdcard_mount();
}
```

sdkconfig 中 BSP SD 挂载点：

```text
CONFIG_BSP_SD_MOUNT_POINT="/sdcard"
```

所以 TF 卡可以用。

### 6.1 推荐用途

TF 卡适合：

- 保存设备控制日志。
- 保存从机传感器历史数据。
- 保存小智聊天日志或调试日志。
- 保存 UI 截图。
- 保存用户配置备份。
- 保存非关键图片、背景、扩展字体。
- 保存 OTA 下载临时文件或大资源缓存。

### 6.2 不推荐用途

TF 卡不建议用于：

- 系统启动必须加载的唯一字体。
- 小智聊天页核心图标。
- 智能家居控制按钮核心资源。
- 实时控制命令队列的唯一持久化。

原因：

- 用户可能拔卡。
- 卡可能格式不兼容。
- 卡读写速度不稳定。
- FATFS 写入时机不当会造成卡顿。

### 6.3 使用策略

推荐启动时检测：

```text
/sdcard mounted successfully
```

若成功：

- 开启日志落盘。
- 开启历史数据缓存。
- 允许加载扩展资源。

若失败：

- UI 正常启动。
- 控制功能正常。
- 只关闭日志/扩展资源。
- 在设置页显示“TF 卡未挂载”。

## 7. 从机与主机控制链路

从机已经按楼层拆分：

```text
一楼：xiaozhi_slave_Firstfloor
二楼：xiaozhi_slave_Secondfloor
三楼：xiaozhi_slave_Thirdfloor
```

融合后的主机应同时支持：

1. 触摸 UI 控制。
2. 小智语音/AI MCP 控制。
3. 从机心跳和状态上报。
4. 设置页中显示 MQTT/从机连接状态。

推荐主机统一维护设备表：

```text
device_id = 1, name = 一楼设备
device_id = 2, name = 二楼设备
device_id = 3, name = 三楼设备
```

所有控制命令通过统一 Controller 发出，避免 UI 和 AI 两套逻辑不一致。

推荐保留协议：

```c
typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} __attribute__((packed)) iot_command_packet_t;
```

该结构简单、体积小、适合 MQTT 二进制 payload，也与现有从机兼容。

## 8. 主要风险和处理方案

### 8.1 两套 LVGL 初始化冲突

风险：

旧 demo 有自己的 `lvgl_adapter_init()` 和 BSP display 初始化。

处理：

不迁移旧 demo 初始化入口。主项目只保留现有 `esp_lvgl_port` + `MipiLcdDisplay` 初始化。

### 8.2 两套菜单栏冲突

风险：

旧智能家居 UI 内部已经有左侧菜单栏，用户又希望小智和家居通过左侧菜单栏切换，可能出现双菜单。

处理：

推荐最终只保留一套总菜单栏：

```text
小智 / 总览 / 控制 / 网络 / 设置
```

旧智能家居菜单逻辑可复用，但菜单项扩展为包含“小智”。

### 8.3 小智聊天 UI 被智能家居页面清空

风险：

旧 `UI_Manager_Init()` 当前会清理 `lv_screen_active()`。

处理：

必须改为传入父容器，不允许智能家居 UI 操作整个 screen。

### 8.4 MQTT 回调直接操作 LVGL

风险：

LVGL 不是任意线程安全，直接在 MQTT 回调中改 UI 会引发随机崩溃。

处理：

MQTT 回调只更新模型或投递事件；LVGL 操作必须在持有 `DisplayLockGuard` 或 LVGL port lock 的上下文中执行。

### 8.5 字体重复和 Flash 超限

风险：

旧 demo 有多套中文字体和图标字体，主项目也有字体资源。

处理：

先迁移必要字体，编译后检查 app 分区大小；能复用主项目字体就复用。

### 8.6 TF 卡依赖过强

风险：

拔卡后 UI 启动失败或资源缺失。

处理：

核心 UI 资源内置，TF 卡只做扩展。

## 9. 推荐验收标准

### 9.1 编译验收

执行：

```powershell
idf.py build
```

验收：

- 编译通过。
- 无 LVGL v8/v9 API 错误。
- 无重复符号。
- 无 include 路径冲突。
- app 分区未超限。

### 9.2 启动验收

设备启动后：

- LCD 正常点亮。
- 触摸正常。
- 小智原有聊天界面正常显示。
- 左侧菜单显示。
- 默认页正常进入。
- 无重启、无 assert、无 watchdog。

### 9.3 页面切换验收

测试：

- 点击“小智”进入聊天页。
- 点击“总览”进入智能家居总览。
- 点击“控制”进入控制页。
- 点击“网络”进入网络页。
- 点击“设置”进入设置页。
- 连续快速切换 50 次。

验收：

- 页面不花屏。
- 对象不重叠。
- 触摸不失效。
- 聊天页状态不丢失。
- 智能家居页状态不乱跳。

### 9.4 小智功能验收

测试：

- 唤醒小智。
- 对话。
- 显示字幕。
- 显示状态。
- 显示表情。
- 切到智能家居页后继续对话，再切回小智页。

验收：

- 小智业务逻辑不受影响。
- `SetChatMessage()`、`SetEmotion()`、`SetStatus()` 都正常。

### 9.5 智能家居控制验收

测试：

- 三个从机上线 announce。
- 主机接收 heartbeat。
- UI 控制一楼/二楼/三楼灯。
- UI 控制继电器。
- UI 控制舵机。
- 广播全开/全关。

验收：

- MQTT Topic 正确。
- 从机响应正确。
- UI 状态同步。
- AI 控制和 UI 控制状态一致。

### 9.6 TF 卡验收

测试：

- 插卡启动。
- 不插卡启动。
- 运行时记录日志。
- 设置页查看 TF 卡状态。

验收：

- 插卡时挂载成功并可写日志。
- 不插卡时系统正常启动。
- TF 卡失败不影响聊天和家居控制。

## 10. 最终建议

该项目适合按“主项目为核心、旧 demo 为 UI 模块来源”的方式融合。

最重要的工程边界是：

- 只保留一套 LVGL 初始化。
- 只保留一套 LCD/触摸初始化。
- 只保留一套小智显示抽象。
- 智能家居 UI 只能在父容器内创建，不允许清空全局 screen。
- IoT 控制必须统一到主项目服务层，UI 和 AI 共用同一设备模型。
- TF 卡作为增强能力使用，不作为核心依赖。

按上述方案实施后，ESP-P4 Function EV Board 可以同时承担小智聊天界面和智能家居控制界面，并通过左侧菜单栏在两类功能之间自然切换。
