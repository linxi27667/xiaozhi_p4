明白了,广州天气是你故意要的,我之前误判为 bug,抱歉。

现在让我先深度调研几个关键点,然后编写完整实现计划。
用户要求我编写一个完整的计划,把摄像头人脸识别加入项目。需求:
1. 上电弹窗,两个选项:人脸识别登录 / 密码登录(密码固定 0000)
2. 登录后才能进行系统交互
3. 选人脸识别时,弹窗变成人脸识别模式,显示摄像头画面,识别到人脸后进入系统
4. 右上角有返回按钮,可重新选择登录方式
5. 打通小智 MCP 的识图逻辑
6. 评判可行性并编写实现计划

用户也澄清了:广州天气是他要的,不是 bug。

这是一个需要规划的复杂任务,我应该使用 writing-plans skill 来编写实现计划。但在写计划前,我需要先深度调研一些关键信息:
1. 当前启动流程和 UI 初始化时机
2. 当前 MCP 识图逻辑现状
3. esp-face 组件可用性
4. 人脸识别模型存储方案
5. LVGL 弹窗实现参考(smart_home_alarm_ui.c)

让我先并行启动几个调研任务,然后编写完整计划。

实际上,用户明确要求"编写一个完整的计划",我应该使用 writing-plans skill。但首先需要收集足够信息。

让我先做关键调研,然后写计划。由于这是规划阶段,不需要修改代码,我应该:
1. 调研当前启动流程和 UI 初始化
2. 调研当前 MCP 识图逻辑
3. 调研 esp-face 可行性和集成方式
4. 编写完整实现计划

让我先并行启动调研任务。

我将对 xiaozhi-for-p4 项目的启动流程和 UI 初始化时机进行深度调研。这是一个纯研究任务,不修改任何代码。让我并行启动多个搜索来高效完成调研。
The user wants a deep investigation of the xiaozhi-for-p4 project's startup flow and UI initialization timing, to prepare for adding a "login popup". This is a research/investigation task, not a code modification task.

Let me check the available skills first. The user's task is about investigating code in an existing codebase - this is a search/research task. Looking at the skills:
- "embedded-architect" - Provides expert guidance on embedded system architecture design. This could be relevant since the user is asking about embedded system architecture.
- "esp32" - Expert Embedded Systems guidance for ESP32 hardware, ESP-IDF firmware. This is highly relevant since the project is ESP32-P4 based.

However, the user is asking for a code investigation/search task, not really asking for architecture guidance or ESP32 expertise. The user explicitly says "请详细列出代码位置、调用链、可扩展点。不要修改任何代码。" (Please list code locations, call chains, extension points in detail. Do not modify any code.)

This is primarily a file search and code analysis task. The skills are about providing guidance, but the user wants me to investigate the actual code. I should use the search tools (Grep, Read, Glob, SearchCodebase) to investigate the codebase.

Wait, the system reminder says: "Before starting any task, first review the Skill tool description to check if any skill in its <available_skills> is relevant to the <user_input> intent. When a skill is relevant, you must invoke the Skill tool IMMEDIATELY as your first action."

The esp32 skill seems relevant since this is an ESP32-P4 project. But the task is really about code investigation. Let me think... The esp32 skill description says "Expert Embedded Systems guidance for ESP32 hardware, ESP-IDF firmware, and PlatformIO projects." This is about guidance, not investigation.

Actually, looking more carefully, the task is purely investigative - reading and understanding existing code. The skills are more about providing expertise/guidance. I think the most appropriate approach is to use the search tools directly to investigate the codebase.

But the system reminder is quite emphatic about using skills. Let me reconsider... The "embedded-architect" skill says "Provides expert guidance on embedded system architecture design, emphasizing Hybrid Layered Architecture". This could be relevant for understanding the architecture.

Hmm, but the user's task is very specific - investigate the startup flow and UI initialization. This is a search task. I'll proceed with the search tools directly since they're the most appropriate for this investigation task. The skills don't seem to directly help with code investigation.

Actually, let me re-read the rules: "When a skill is relevant, you must invoke the Skill tool IMMEDIATELY as your first action." The key word is "relevant". For a pure code investigation task, I don't think the skills are directly relevant - they're more about creating/designing, not investigating existing code.

I'll proceed with parallel searches to investigate the codebase efficiently. Let me start with multiple parallel searches:

1. Find application.cc and look at Initialize()
2. Find lcd_display.cc and look at SetupUI()
3. Find smart_home_alarm_ui.c
4. Find ui_welcome_popup.c
5. Find ui_manager.c
6. Find SsidManager
7. Find kDeviceState definitions
8. Find SmartHomeTasksStart

Let me start with parallel searches.

我已经获取了 application.cc 的完整内容。现在并行读取其他关键文件。
Good, I have the application.cc content. Now let me read the lcd_display.cc, smart_home_alarm_ui.c, ui_welcome_popup.c, ui_manager.c, and smart_home_tasks.cc in parallel.

继续并行调研其他关键文件:smart_home_tasks.cc、SsidManager、DeviceState 定义、app_main 入口、ui_events.c。
Excellent! I have a lot of information now. Let me continue investigating:
1. smart_home_tasks.cc - to understand SmartHomeTasksStart
2. SsidManager - for NVS reference
3. DeviceState enum and state machine
4. app_main entry point
5. ui_events.c for event mechanism
6. settings.h for NVS usage

Let me search for these in parallel.

继续并行读取 device_state.h、device_state_machine.h、application.h,并搜索 SsidManager 和 app_main。
Let me read the device_state.h, device_state_machine.h, the application.h, and search for SsidManager and app_main entry point.

继续并行读取 main.cc、application.h、ui_events.c,以及搜索 SsidManager 实现和 Settings 类。
Now let me read main.cc (the entry point), the application.h, and look at SsidManager in wifi_board.cc. Also need to look at ui_events.c for the event mechanism.

继续并行读取 application.h、settings.h、settings.cc,以及搜索 SsidManager 类定义。
Now let me read the application.h, settings.h, and look at SsidManager. Let me also look at the wifi_board.cc around line 90 to understand SsidManager usage.

继续并行搜索 SsidManager 类定义、查看 lcd_display.h、ui_events.h、ui_welcome_popup.h。
Let me search for SsidManager class definition and look at the wifi_board.cc around the SsidManager usage to understand the API. Also let me look at the lcd_display.h to understand the class structure.

继续并行读取 ui_events.h、ui_welcome_popup.h、smart_home_alarm_ui.h,并搜索 SsidManager 类定义。
Let me read the ui_events.h, ui_welcome_popup.h, and look at the wifi_board.cc to find the SsidManager class definition (it might be in a header). Also let me look at the smart_home_alarm_ui.h.

继续并行读取 smart_home_alarm_ui.h、查看 wifi_board.cc 中 SsidManager 的引用,以及搜索 SsidManager 类定义。
Let me search for the SsidManager class definition. It seems to be referenced but I haven't found the class definition yet. Let me search for it more broadly and also look at the smart_home_alarm_ui.h and the wifi_board.cc to understand SsidManager.

继续并行搜索 SsidManager 在 components 中的位置、查看 wifi_board.cc 的 StartNetwork 流程,以及 board.h。
The SsidManager is in an external library (likely in a components folder). Let me search for it. Also, let me look at the wifi_board.cc to understand the StartNetwork flow and check the board.h for GetDisplay.

继续并行查看 wifi_board.cc 完整的 StartNetwork 流程、esp-p4-function-ev-board.cc 中的网络启动,以及 lvgl_display.h 基类。
Let me look at the wifi_board.cc more broadly to understand the SsidManager and the network flow, and also look at the esp-p4-function-ev-board to understand the P4 board's StartNetwork. Also let me check the lvgl_display.h to understand the base class.

继续并行查看 esp-p4-function-ev-board.cc、lvgl_display.h,以及搜索 lv_msgbox_create 的使用方式。
Let me look at the esp-p4-function-ev-board.cc to understand the P4 board's structure. Also let me look at the lvgl_display.h to understand the Display base class and its SetupUI.

继续并行读取 display.h、smart_home_tasks.h、ui_manager.h,以及查看 esp-p4-function-ev-board.cc 的构造函数和 display 创建。
Now I have enough information. Let me also look at the display.h to understand the base class SetupUI, and look at the esp-p4-function-ev-board.cc to understand how the board is set up (specifically how display is created and when). Let me also look at the LV_OBJ_FLAG_FLOATING usage and check the page_data.c briefly to understand page lifecycle.

Let me also check the smart_home_tasks.h and the ui_manager.h.

继续并行读取 display.h、ui_manager.h、esp-p4-function-ev-board.cc 构造函数,以及 ui_kit.c 中的 popup 实现。
Let me look at the display.h, ui_manager.h, and the ESP32P4FunctionEvBoard constructor to understand the initialization flow. Also let me look at the ui_kit.c popup implementation for another reference.

最后并行查看 ui_kit.c 的 popup 函数签名、settings.cc 的 NVS 实现,以及 page_data.c 的页面生命周期。
I now have a comprehensive understanding of the codebase. Let me also quickly check the ui_kit.c popup function signature and the page_data.c for page lifecycle reference. Let me also check the settings.cc to understand NVS usage.

我已经完成了对 xiaozhi-for-p4 项目启动流程和 UI 初始化时机的深度调研。以下是详细的分析报告。

---

# xiaozhi-for-p4 启动流程与 UI 初始化调研报告

## 1. 启动流程时序

### 1.1 入口点 `main/main.cc:14-29`

`app_main()` 的完整流程:
1. `nvs_flash_init()` — 初始化 NVS Flash(登录状态持久化的基础)
2. `Application::GetInstance().Initialize()` — 初始化应用
3. `Application::GetInstance().Run()` — 主事件循环,永不返回

### 1.2 `Application::Initialize()` 完整流程 `main/application.cc:64-183`

按顺序执行:

| 步骤 | 行号 | 操作 | 说明 |
|------|------|------|------|
| 1 | 65-66 | `Board::GetInstance()` + `SetDeviceState(kDeviceStateStarting)` | Board 构造(含 LVGL 任务启动) |
| 2 | 69-70 | `display->SetupUI()` | **同步**创建侧栏 + smart_home_page_ 容器 |
| 3 | 73-75 | `audio_service_.Initialize(codec)` + `Start()` | 音频服务 |
| 4 | 77-87 | 设置 AudioServiceCallbacks | 唤醒词/VAD/发送队列回调 |
| 5 | 90-92 | `state_machine_.AddStateChangeListener(...)` | 状态变化监听 → `MAIN_EVENT_STATE_CHANGED` |
| 6 | 95 | `esp_timer_start_periodic(clock_timer_handle_, 1000000)` | 1 秒时钟触发 `MAIN_EVENT_CLOCK_TICK` |
| 7 | 98-101 | MCP 工具注册 | `AddCommonTools` + `AddUserOnlyTools` + `SmartHomeMcp_RegisterTools` |
| 8 | 104-162 | `board.SetNetworkEventCallback(...)` | 网络事件回调(Scanning/Connecting/Connected/Disconnected) |
| 9 | **166** | **`SmartHomeTasksStart()`** | 启动智能家居任务层(MQTT+天气,Core1,prio5,stack8K) |
| 10 | 169 | `display->UpdateStatusBar(true)` | 立即更新状态栏 |
| 11 | 171-177 | `xTaskCreate("network_start", 8192, prio5)` | **异步**创建网络启动任务 |

### 1.3 LVGL 任务启动时机

LVGL 任务在 **Board 构造函数** 中启动,早于 `SetupUI()`:

`main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc:162-174`
```cpp
ESP32P4FunctionEvBoard() : boot_button_(GPIO_NUM_35) {
    InitializeI2cBuses();
    InitializeLCD();      // ← 这里 new MipiLcdDisplay()
    InitializeButtons();
    InitializeTouch();
    InitializeSdCard();
    ui_asset_service_preload_required();
    InitializeCamera();
    InitializeFonts();
    GetBacklight()->RestoreBrightness();
}
```

`InitializeLCD()` (line 70): `display_ = new MipiLcdDisplay(handles.io, handles.panel, 1024, 600, ...)`

`MipiLcdDisplay` 构造函数 `main/display/lcd_display.cc:256-306` 中调用:
- `lv_init()` (line 262)
- `lvgl_port_init(&port_cfg)` (line 267) — **LVGL 任务在此启动**
- `lvgl_port_add_disp_dsi(...)` (line 297)

**结论**:LVGL 任务在 `Application::Initialize()` 第一行 `Board::GetInstance()` 时就已启动,远早于 `display->SetupUI()`。

### 1.4 完整时序图

```
app_main()
├─ nvs_flash_init()
└─ Application::Initialize()
   ├─ Board::GetInstance()  [Board 构造函数]
   │  └─ ESP32P4FunctionEvBoard()
   │     ├─ InitializeI2cBuses()
   │     ├─ InitializeLCD()
   │     │  └─ new MipiLcdDisplay()
   │     │     ├─ lv_init()
   │     │     ├─ lvgl_port_init()  [LVGL 任务启动, prio 默认, stack 12K]
   │     │     └─ lvgl_port_add_disp_dsi()
   │     ├─ InitializeTouch()  [触摸 indev 注册]
   │     └─ ...
   ├─ SetDeviceState(kDeviceStateStarting)
   ├─ display->SetupUI()  [创建侧栏 + smart_home_page_ 容器]
   ├─ audio_service_.Initialize() + Start()
   ├─ SmartHomeTasksStart()  [MQTT 任务 Core1 启动,延迟5秒后开始轮询]
   ├─ display->UpdateStatusBar(true)
   └─ xTaskCreate("network_start")  [异步网络连接]
└─ Application::Run()  [主事件循环,永不返回]
```

---

## 2. UI 初始化

### 2.1 `SetupUI()` 实现 `main/display/lcd_display.cc:1225-1418`(非微信模式)

防重复机制:`if (setup_ui_called_) return` (line 1227-1230)

执行顺序:
1. `Display::SetupUI()` 标记 `setup_ui_called_ = true` (line 1232)
2. `DisplayLockGuard lock(this)` — 获取 LVGL 锁 (line 1233)
3. 创建 `container_` (line 1245) — 全屏背景
4. 创建 `emoji_box_` + `emoji_label_` (line 1254-1264) — 居中显示 `FONT_AWESOME_MICROCHIP_AI` AI logo
5. 创建 `preview_image_` (line 1271) — 预览图(初始 HIDDEN)
6. 创建 `top_bar_` (line 1277) — 顶部状态栏(网络/静音/电池图标)
7. 创建 `status_bar_` (line 1320) — 状态文字层
8. 创建 `notification_label_` + `status_label_` (line 1332-1346) — `status_label_` 初始显示 `Lang::Strings::INITIALIZING`
9. 创建 `bottom_bar_` + `chat_message_label_` (line 1349-1402) — 底部聊天消息栏(初始 HIDDEN)
10. 创建 `low_battery_popup_` (line 1404-1415) — 低电量弹窗(初始 HIDDEN)
11. **`CreateSmartHomeShell()`** (line 1417) — 创建侧栏 + 智能家居容器

### 2.2 侧栏(sidebar)创建 `CreateSmartHomeShell()` `lcd_display.cc:588-772`

- `shell_created_at_us_ = esp_timer_get_time()` (line 590) — 记录创建时间(用于启动触摸保护)
- 调整 `container_/top_bar_/status_bar_` 位置,给侧栏让出 `kShellSidebarWidth = 128px` (line 594-612)
- 创建 `side_bar_` (line 636-645):128px 宽,白色背景,右边框
- 创建 `nav_col` flex 容器 (line 647-656)
- 创建 8 个导航按钮 (line 691-721):聊天/总览/控制/灯光/场景/环境/网络/设置
- 创建唤醒按钮 (line 724-758):底部蓝色,68px 高,点击触发 `ToggleChatState()`
- **创建 `smart_home_page_` 容器** (line 760-766):初始 `LV_OBJ_FLAG_HIDDEN`
- `lv_obj_move_foreground(side_bar_)` (line 768)
- 订阅 `UI_EVENT_LANG_CHANGED` 和 `UI_EVENT_PAGE_SWITCHED` (line 769-770)
- `RefreshShellNav(ShellPage::Chat)` (line 771) — 默认选中聊天页

### 2.3 `smart_home_page_` 容器何时初始化第一个页面

**延迟初始化机制** `lcd_display.cc:496-503`:
```cpp
void LcdDisplay::EnsureSmartHomeUi() {
    if (smart_home_ui_initialized_ || !smart_home_page_) {
        return;
    }
    UI_Manager_Init(smart_home_page_);
    smart_home_ui_initialized_ = true;
}
```

`EnsureSmartHomeUi()` 只在 `SwitchShellPage()` 切换到非 Chat 页时调用 (line 551):
```cpp
if (smart_home) {
    lv_obj_remove_flag(smart_home_page_, LV_OBJ_FLAG_HIDDEN);
    ...
    EnsureSmartHomeUi();  // ← 首次切换时才初始化
    switch (page) { ... UI_Manager_Switch_Page(...); }
}
```

**结论**:启动时 `smart_home_page_` 容器创建但 HIDDEN,`UI_Manager_Init()` **不会立即调用**,只有用户首次点击侧栏非聊天按钮时才初始化。

### 2.4 `UI_Manager_Init()` `main/smart_home/ui/ui_manager.c:237-283`

- `memset(&s_app_ctx, 0, ...)` + `current_page = UI_PAGE_DATA`
- 初始化子系统:`ui_events_init()` / `ui_styles_init()` / `ui_i18n_init()` / `ui_asset_service_init()` / `device_model_init()` / `smart_home_alarm_ui_init()` / `ui_welcome_popup_init()`
- `lv_obj_clean(scr)` 清空 parent
- `create_embedded_content_area(scr)` 创建内容区
- 订阅 `UI_EVENT_LANG_CHANGED`
- 设置 `switch_pending = 1`, `target_page = UI_PAGE_DATA`
- 创建 50ms 轮询定时器 `s_poll_timer = lv_timer_create(poll_timer_cb, 50, NULL)`

首次 Poll 时 `do_switch_page()` (line 123-147) 调用 `page_data_create()` 创建总览页。

### 2.5 启动画面/锁屏概念

**当前没有专门的"启动画面"或"锁屏"概念**,但有类似机制:

1. **启动画面**:启动时 `emoji_label_` 居中显示 `FONT_AWESOME_MICROCHIP_AI` 图标,`status_label_` 显示 "INITIALIZING"
2. **启动触摸保护** `lcd_display.cc:35, 403-410`:
   ```cpp
   static constexpr int kShellStartupTouchGuardMs = 1000;
   if (page != ShellPage::Chat && display->shell_created_at_us_ > 0) {
       int64_t elapsed_us = esp_timer_get_time() - display->shell_created_at_us_;
       if (elapsed_us < kShellStartupTouchGuardMs * 1000) {
           ESP_LOGW(TAG, "Ignore smart-home shell touch during startup guard");
           return;
       }
   }
   ```
   创建侧栏后 1 秒内忽略非聊天页的点击,防止启动时误触。

---

## 3. 弹窗实现参考

### 3.1 火警弹窗 `main/smart_home/ui/core/smart_home_alarm_ui.c`

**使用 `lv_msgbox_create(NULL)` 内置模态** (line 61):
```cpp
s_modal = lv_msgbox_create(NULL);  // parent=NULL 自动创建模态 + 背景遮罩
lv_msgbox_add_title(s_modal, "火灾警报");
lv_msgbox_add_text(s_modal, body);
lv_obj_t *btn = lv_msgbox_add_footer_button(s_modal, "紧急处理");
lv_obj_add_event_cb(btn, on_alarm_ack, LV_EVENT_CLICKED, NULL);
```

样式 (line 95-144):
- 红色背景 `0xC62828`,边框 `0xB71C1C`,圆角 12
- header/content/footer 透明背景
- 按钮白色背景 + 红色文字

关闭方式 (line 33-40):
```cpp
static void close_modal(void) {
    if (s_modal) {
        lv_msgbox_close_async(s_modal);  // 异步关闭,安全
        s_modal = NULL;
    }
}
```

触发机制:
- `ui_event_subscribe(UI_EVENT_FIRE_ALARM, on_fire_alarm_event, NULL)` (line 208)
- 500ms 轮询 `device_model` 检查火警状态 (line 211)
- `smart_home_alarm_ui_set_fire()` → `ui_event_publish(UI_EVENT_FIRE_ALARM)`

### 3.2 欢迎弹窗 `main/smart_home/ui/services/ui_welcome_popup.c`

**手动构建全屏 overlay + 卡片** (line 51-95):
```cpp
lv_obj_t *scr = lv_screen_active();
s_popup = lv_obj_create(scr);
lv_obj_remove_style_all(s_popup);
lv_obj_set_size(s_popup, lv_pct(100), lv_pct(100));
lv_obj_add_flag(s_popup, LV_OBJ_FLAG_FLOATING);       // ← 关键:脱离布局,z-order 最顶
lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_set_style_bg_color(s_popup, lv_color_black(), 0);
lv_obj_set_style_bg_opa(s_popup, LV_OPA_20, 0);       // 20% 黑色遮罩

lv_obj_t *box = lv_obj_create(s_popup);               // 卡片
lv_obj_set_size(box, 360, 168);
lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 42);
// ... 样式 ...
```

关闭方式 (line 38-43):
```cpp
static void close_popup(void) {
    if (!s_popup) return;
    lv_obj_delete(s_popup);
    s_popup = NULL;
}
```

触发机制 (line 97-108):
- `ui_event_subscribe(UI_EVENT_SCENE_CHANGED, on_scene_changed, NULL)`
- 检查 `model->current_scene == IOT_SCENE_HOME` → `show_home_popup()`

### 3.3 `ui_kit.c` 通用 popup 工厂 `main/smart_home/ui/core/ui_kit.c:369-424`

`ui_kit_popup_create(parent, title, accent_color)` 返回 `ui_kit_popup_t` 结构体,包含 overlay/card/title_label/close_btn 等字段,模式与欢迎弹窗一致。

### 3.4 `lv_msgbox_create` 使用方式

项目中仅火警弹窗使用 `lv_msgbox_create(NULL)`:
- `parent=NULL` → 自动创建模态,带背景遮罩(backdrop),拦截背景点击
- LVGL 内置模态对话框,自动管理 z-order
- 但灵活性不如手动构建(欢迎弹窗方式)

### 3.5 启动时显示全屏模态弹窗拦截所有交互的方案

**方案A:使用 `lv_msgbox_create(NULL)`**(简单,自动模态)
```cpp
lv_obj_t *mbox = lv_msgbox_create(NULL);
lv_msgbox_add_title(mbox, "登录");
lv_msgbox_add_text(mbox, "请输入密码");
lv_obj_t *btn = lv_msgbox_add_footer_button(mbox, "登录");
lv_obj_add_event_cb(btn, on_login, LV_EVENT_CLICKED, NULL);
```

**方案B:手动构建全屏 floating overlay**(灵活,参考欢迎弹窗)
```cpp
lv_obj_t *scr = lv_screen_active();
lv_obj_t *overlay = lv_obj_create(scr);
lv_obj_remove_style_all(overlay);
lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
lv_obj_add_flag(overlay, LV_OBJ_FLAG_FLOATING);       // z-order 最顶
lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);      // 拦截点击
lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);       // 50% 遮罩
// 在 overlay 上创建登录表单...
```

---

## 4. 输入拦截

### 4.1 LVGL 让弹窗拦截所有点击(包括侧栏)

**关键点**:侧栏 `side_bar_` 和 `smart_home_page_` 都是 `lv_screen_active()` 的直接子对象 (lcd_display.cc:636, 760)。

拦截方法:
1. **`LV_OBJ_FLAG_FLOATING`** — 使对象脱离正常布局,浮在最顶层
2. **全屏覆盖** — `lv_obj_set_size(obj, lv_pct(100), lv_pct(100))` 覆盖整个屏幕(包括侧栏区域)
3. **`LV_OBJ_FLAG_CLICKABLE`** — 拦截点击事件,不传递到下层

### 4.2 `LV_OBJ_FLAG_FLOATING` 的效果

项目中使用位置:
- `ui_welcome_popup.c:59` — 欢迎弹窗 overlay
- `ui_kit.c:379` — 通用 popup overlay
- `page_light.c:571` — 灯光预览(局部)

效果:
- 对象脱离父对象的布局流
- z-order 提升到最顶(在所有非 floating 对象之上)
- 但仍受父对象裁剪 — **所以必须挂在 `lv_screen_active()` 上,而不是 `smart_home_page_` 上**

### 4.3 "登录前不能操作任何 UI" 实现要点

1. 弹窗必须挂在 `lv_screen_active()` 上(不是 `smart_home_page_` 或 `side_bar_`)
2. 使用 `LV_OBJ_FLAG_FLOATING` + 全屏尺寸覆盖侧栏
3. 设置 `LV_OBJ_FLAG_CLICKABLE` 拦截点击
4. 登录成功后 `lv_obj_delete(overlay)` 释放拦截

**注意**:火警弹窗用 `lv_msgbox_create(NULL)` 自动挂到 active screen 的 layer top,也能拦截侧栏,但样式较固定。

---

## 5. 状态持久化

### 5.1 NVS 存储"是否已登录"状态

**`Settings` 类** `main/settings.h` + `main/settings.cc` 封装了 NVS 操作:

```cpp
// 读
Settings settings("auth", false);  // 只读
bool logged_in = settings.GetBool("logged_in", false);
std::string token = settings.GetString("token", "");

// 写
Settings settings("auth", true);   // 读写
settings.SetBool("logged_in", true);
settings.SetString("token", "xxx");
// 析构时自动 nvs_commit()
```

API:`GetString/SetString/GetInt/SetInt/GetBool/SetBool/EraseKey/EraseAll`

已有命名空间参考:
- `"display"` — 主题 (lcd_display.cc:93)
- `"assets"` — 资源下载URL (application.cc:376)
- WiFi SSID — 通过 `SsidManager`(外部库)

### 5.2 是否需要每次上电都登录

两种方案:

**方案1:持久化登录(推荐,类似 SsidManager 模式)**
- NVS 存 `logged_in=true` + token
- 上电检查 token 有效性,有效则跳过登录
- token 过期则重新登录

**方案2:每次上电登录**
- 不持久化,或每次上电校验
- 安全性高但用户体验差

`SsidManager` 参考 `main/boards/common/wifi_board.cc:89-104`:
```cpp
void WifiBoard::TryWifiConnect() {
    auto& ssid_manager = SsidManager::GetInstance();
    bool have_ssid = !ssid_manager.GetSsidList().empty();
    if (have_ssid) {
        // 有 SSID,自动连接
        esp_timer_start_once(connect_timer_, ...);
        WifiManager::GetInstance().StartStation();
    } else {
        // 无 SSID,进入配置模式
        StartWifiConfigMode();
    }
}
```
这是典型的"持久化 + 自动重连"模式,可参考用于登录状态。

### 5.3 NVS 初始化时机

`main/main.cc:17-23`: `nvs_flash_init()` 在 `app_main()` 最开始调用,远早于 `Application::Initialize()`,所以登录状态检查可以在 `Initialize()` 任何位置进行。

---

## 6. 现有"模式"概念

### 6.1 DeviceState 枚举 `main/device_state.h`

```cpp
enum DeviceState {
    kDeviceStateUnknown,         // 0 未知
    kDeviceStateStarting,        // 1 启动中
    kDeviceStateWifiConfiguring, // 2 WiFi 配置中
    kDeviceStateIdle,            // 3 空闲
    kDeviceStateConnecting,      // 4 连接中
    kDeviceStateListening,       // 5 监听中
    kDeviceStateSpeaking,        // 6 说话中
    kDeviceStateUpgrading,       // 7 升级中
    kDeviceStateActivating,      // 8 激活中
    kDeviceStateAudioTesting,    // 9 音频测试
    kDeviceStateFatalError       // 10 致命错误
};
```

### 6.2 状态机工作原理 `main/device_state_machine.cc`

**状态转换规则** (line 34-102) 严格校验:
- `kDeviceStateUnknown` → `kDeviceStateStarting`
- `kDeviceStateStarting` → `kDeviceStateWifiConfiguring` / `kDeviceStateActivating`
- `kDeviceStateWifiConfiguring` → `kDeviceStateActivating` / `kDeviceStateAudioTesting`
- `kDeviceStateIdle` → `kDeviceStateConnecting` / `kDeviceStateListening` / `kDeviceStateSpeaking` / `kDeviceStateActivating` / `kDeviceStateUpgrading` / `kDeviceStateWifiConfiguring`
- `kDeviceStateFatalError` → 不可转出

**转换流程** (line 108-131):
1. `TransitionTo(new_state)` 检查 `IsValidTransition()`
2. 无效转换 → 返回 false + 日志警告
3. 有效转换 → `current_state_.store(new_state)` + `NotifyStateChange()`
4. `NotifyStateChange()` 调用所有注册的回调

### 6.3 状态变化时 UI 如何响应

**注册监听** `application.cc:90-92`:
```cpp
state_machine_.AddStateChangeListener([this](DeviceState old_state, DeviceState new_state) {
    xEventGroupSetBits(event_group_, MAIN_EVENT_STATE_CHANGED);
});
```

**主循环处理** `application.cc:224-226`:
```cpp
if (bits & MAIN_EVENT_STATE_CHANGED) {
    HandleStateChangedEvent();
}
```

**`HandleStateChangedEvent()`** `application.cc:875-947` 根据 new_state 切换 UI:
- `kDeviceStateIdle`: `SetStatus(STANDBY)` + `ClearChatMessages()` + `SetEmotion("neutral")` + 启用唤醒词
- `kDeviceStateConnecting`: `SetStatus(CONNECTING)` + `SetEmotion("neutral")`
- `kDeviceStateListening`: `SetStatus(LISTENING)` + 启用语音处理
- `kDeviceStateSpeaking`: `SetStatus(SPEAKING)` + 禁用语音处理
- `kDeviceStateWifiConfiguring`: 禁用语音处理 + 禁用唤醒词

### 6.4 扩展 `kDeviceStateLocked` 的可行性

**可以扩展**,需要修改 3 个文件:

1. **`main/device_state.h`** — 添加 `kDeviceStateLocked` 枚举值
2. **`main/device_state_machine.cc`**:
   - `STATE_STRINGS[]` 数组添加 "locked"
   - `IsValidTransition()` 添加规则:
     - `kDeviceStateStarting` → `kDeviceStateLocked`(启动后进入锁屏)
     - `kDeviceStateLocked` → `kDeviceStateIdle`(登录成功)
     - `kDeviceStateLocked` → `kDeviceStateWifiConfiguring`(可选)
3. **`main/application.cc`** `HandleStateChangedEvent()` 添加 `case kDeviceStateLocked:`:
   - 显示登录弹窗
   - 禁用音频处理 + 唤醒词
   - `display->SetStatus("已锁定")`

**注意**:状态机是严格校验的,添加新状态必须更新转换规则,否则 `TransitionTo()` 会失败。

---

## 7. 可扩展点总结

### 7.1 登录弹窗插入点(推荐顺序)

**位置A:`Application::Initialize()` 中 `display->SetupUI()` 之后** `application.cc:70 后`
- 优点:LVGL 已就绪,侧栏和容器已创建;`SmartHomeTasksStart()` 和 `network_start` 还未启动,可以决定是否启动
- 缺点:在 C++ 侧,需要调用 C 层 UI 函数

**位置B:`LcdDisplay::CreateSmartHomeShell()` 末尾** `lcd_display.cc:772 前`
- 优点:此时侧栏、smart_home_page_ 都已创建,可以在 `lv_screen_active()` 上叠加 overlay
- 缺点:在 UI 层,无法直接控制状态机

**位置C:`UI_Manager_Init()` 中** `ui_manager.c:249 后`
- 优点:在 C 层,与现有弹窗(火警/欢迎)同层
- 缺点:`UI_Manager_Init()` 是延迟初始化(首次切换到智能家居页才调用),启动时不会执行

### 7.2 推荐方案

**方案A:基于 DeviceState 扩展(推荐)**
1. 在 `device_state.h` 添加 `kDeviceStateLocked`
2. 在 `device_state_machine.cc` 添加转换规则
3. 在 `Application::Initialize()` line 70 后检查 NVS 登录状态:
   ```cpp
   display->SetupUI();
   Settings auth("auth", false);
   if (!auth.GetBool("logged_in", false)) {
       SetDeviceState(kDeviceStateLocked);  // 触发 UI 显示登录弹窗
   }
   ```
4. 在 `HandleStateChangedEvent()` 添加 `case kDeviceStateLocked:` 显示登录弹窗
5. 登录成功:`SetDeviceState(kDeviceStateIdle)` + `Settings("auth", true).SetBool("logged_in", true)`

**方案B:纯 UI 层弹窗(不改状态机)**
1. 在 `CreateSmartHomeShell()` 末尾检查 NVS 登录状态
2. 创建全屏 floating overlay 拦截所有交互(参考 `ui_welcome_popup.c`)
3. 登录成功后 `lv_obj_delete(overlay)` + 持久化状态

### 7.3 弹窗实现建议

- **参考 `ui_welcome_popup.c`** 的手动构建方式(更灵活,可自定义登录表单)
- 关键属性:`LV_OBJ_FLAG_FLOATING` + `lv_pct(100)` 全屏 + `LV_OBJ_FLAG_CLICKABLE` 拦截
- 挂在 `lv_screen_active()` 上(不是 `smart_home_page_`),确保覆盖侧栏
- 关闭使用 `lv_obj_delete()`(同步)或 `lv_msgbox_close_async()`(异步,用于 msgbox)

### 7.4 关键文件清单

| 文件 | 作用 | 修改概率 |
|------|------|----------|
| `main/main.cc` | 入口,NVS 初始化 | 低 |
| `main/application.cc` | Initialize/Run/状态处理 | 高(方案A) |
| `main/application.h` | Application 类 | 中(可能加方法) |
| `main/device_state.h` | DeviceState 枚举 | 高(方案A) |
| `main/device_state_machine.cc` | 状态转换规则 | 高(方案A) |
| `main/display/lcd_display.cc` | SetupUI/CreateSmartHomeShell | 高(方案B) |
| `main/display/lcd_display.h` | LcdDisplay 类 | 中 |
| `main/settings.h` / `.cc` | NVS 封装 | 低(直接用) |
| `main/smart_home/ui/services/ui_welcome_popup.c` | 欢迎弹窗参考 | 低(参考) |
| `main/smart_home/ui/core/smart_home_alarm_ui.c` | 火警弹窗参考 | 低(参考) |
| `main/smart_home/ui/core/ui_kit.c` | 通用 popup 工厂 | 低(参考/复用) |
| `main/smart_home/ui/core/ui_events.h` / `.c` | 事件机制 | 中(可能加事件) |

### 7.5 注意事项

1. **LVGL 线程安全**:所有 LVGL 调用必须在 `DisplayLockGuard` 保护下,或通过 `Application::Schedule()` 调度到主任务
2. **事件驱动**:UI 更新应通过 `ui_event_publish()` → `UI_Manager_Poll()` 异步分发,不直接调 LVGL
3. **不破坏网络栈**:登录状态不应影响 `Board::GetInstance().GetNetwork()` 调用链
4. **不破坏小智**:登录弹窗不应阻塞 `Application::Run()` 主循环(用事件位而非阻塞)
5. **启动触摸保护**:现有 `kShellStartupTouchGuardMs = 1000ms` 保护,登录弹窗需考虑与此机制的交互
6. **内存安全**:弹窗关闭时确保 `lv_obj_delete()` 释放所有子对象,参考页面生命周期的 `LV_EVENT_DELETE` 回调模式
I now have all the information I need to provide a comprehensive analysis. Let me also quickly check the ui_kit.h for the popup struct definition, and the page_data.c on_delete callback to understand the lifecycle. Let me also check the ui_asset_service_preload_required to understand the preload mechanism.

Actually, I have enough information now. Let me compile the comprehensive analysis.

Let me organize my findings:

## 1. 启动流程时序

### Entry Point: main/main.cc
- `app_main()` → `nvs_flash_init()` → `Application::GetInstance().Initialize()` → `app.Run()`

### Application::Initialize() (main/application.cc:64-183)
1. `SetDeviceState(kDeviceStateStarting)` - 状态机进入 starting
2. `display->SetupUI()` - 同步设置 UI(创建侧栏、smart_home_page_ 容器)
3. `audio_service_.Initialize(codec)` + `audio_service_.Start()` - 音频服务
4. 设置 AudioServiceCallbacks
5. `state_machine_.AddStateChangeListener(...)` - 状态变化监听
6. `esp_timer_start_periodic(clock_timer_handle_, 1000000)` - 1秒时钟
7. MCP 工具注册
8. `board.SetNetworkEventCallback(...)` - 网络事件回调
9. **`SmartHomeTasksStart()`** - 启动智能家居任务层(MQTT + 天气,Core1)
10. `display->UpdateStatusBar(true)` - 立即更新状态栏
11. `xTaskCreate(... "network_start" ...)` - 创建网络启动任务(prio 5, stack 8K)

### LVGL 任务启动时机
- LVGL 任务在 `MipiLcdDisplay` 构造函数中通过 `lvgl_port_init()` 启动
- `MipiLcdDisplay` 在 `ESP32P4FunctionEvBoard::InitializeLCD()` 中创建 (board 构造函数中)
- Board 构造函数在 `Board::GetInstance()` 首次调用时执行(即 `Application::Initialize()` 第一行)
- 所以 LVGL 任务在 `display->SetupUI()` 之前就已经启动了

### 关键时序
```
app_main()
└─ nvs_flash_init()
└─ Application::Initialize()
   ├─ Board::GetInstance()  [Board 构造函数]
   │  └─ ESP32P4FunctionEvBoard()
   │     ├─ InitializeI2cBuses()
   │     ├─ InitializeLCD()  → new MipiLcdDisplay() → lvgl_port_init() [LVGL 任务启动]
   │     ├─ InitializeButtons()
   │     ├─ InitializeTouch()
   │     ├─ InitializeSdCard()
   │     ├─ ui_asset_service_preload_required()
   │     └─ ...
   ├─ SetDeviceState(kDeviceStateStarting)
   ├─ display->SetupUI()  [创建侧栏 + smart_home_page_ 容器,但 UI_Manager_Init 延迟]
   ├─ audio_service_.Initialize() + Start()
   ├─ SmartHomeTasksStart()  [MQTT 任务 Core1 启动]
   ├─ display->UpdateStatusBar(true)
   └─ xTaskCreate("network_start")  [异步网络连接]
└─ Application::Run()  [主事件循环,永不返回]
```

## 2. UI 初始化

### SetupUI() (main/display/lcd_display.cc:1225-1418,非微信模式)
1. 防重复:`if (setup_ui_called_) return`
2. `Display::SetupUI()` 标记已调用
3. `DisplayLockGuard lock(this)` - 获取 LVGL 锁
4. 创建 container_, top_bar_, status_bar_, notification_label_, status_label_
5. 创建 content_(聊天区域)
6. 创建 low_battery_popup_
7. 创建 emoji_image_, emoji_label_(显示 AI logo)
8. **`CreateSmartHomeShell()`** - 创建侧栏 + smart_home_page_ 容器

### CreateSmartHomeShell() (lcd_display.cc:588-772)
1. 记录 `shell_created_at_us_`(用于启动触摸保护)
2. 调整 container_/top_bar_/status_bar_ 位置(给侧栏让出 128px)
3. 创建 `side_bar_`(128px 宽,白色背景)
4. 创建 8 个导航按钮(聊天/总览/控制/灯光/场景/环境/网络/设置)
5. 创建唤醒按钮(底部蓝色)
6. 创建 `smart_home_page_` 容器(初始 HIDDEN)
7. `ui_event_subscribe(UI_EVENT_LANG_CHANGED, ...)`
8. `ui_event_subscribe(UI_EVENT_PAGE_SWITCHED, ...)`
9. `RefreshShellNav(ShellPage::Chat)` - 默认显示聊天页

### smart_home_page_ 容器
- 创建时 `LV_OBJ_FLAG_HIDDEN`,默认隐藏
- 只有用户点击侧栏导航(非聊天)时才显示
- **`EnsureSmartHomeUi()`** 在首次切换到智能家居页时调用 `UI_Manager_Init(smart_home_page_)`

### UI_Manager_Init() (main/smart_home/ui/ui_manager.c:237-283)
- 初始化事件系统、样式、i18n、资产服务、设备模型
- 初始化 `smart_home_alarm_ui_init()`(火警)
- 初始化 `ui_welcome_popup_init()`(欢迎弹窗)
- 创建嵌入式内容区域
- 设置 `switch_pending = 1`, `target_page = UI_PAGE_DATA`
- 创建 50ms 轮询定时器 `s_poll_timer`
- 首次 Poll 时调用 `do_switch_page()` → `page_data_create()`

### 启动画面/锁屏概念
- **没有专门的"启动画面"或"锁屏"概念**
- 启动时显示 `emoji_label_`(FONT_AWESOME_MICROCHIP_AI 图标)在屏幕中央
- `status_label_` 显示 "INITIALIZING"
- 有一个 **启动触摸保护**:`kShellStartupTouchGuardMs = 1000ms`,在 `OnShellNavClicked` 中检查,防止启动时误触智能家居页

## 3. 弹窗实现参考

### 火警弹窗 (smart_home_alarm_ui.c)
- 使用 `lv_msgbox_create(NULL)` - parent=NULL 自动创建模态 + 背景遮罩
- `lv_msgbox_add_title()` - 标题
- `lv_msgbox_add_text()` - 正文
- `lv_msgbox_add_footer_button()` - 按钮
- 通过 `lv_msgbox_close_async(s_modal)` 异步关闭(安全)
- 样式:红色背景 `0xC62828`,白色文字,圆角 12
- 触发:`ui_event_publish(UI_EVENT_FIRE_ALARM)` → `on_fire_alarm_event` 回调
- 500ms 轮询 `device_model` 检查火警状态

### 欢迎弹窗 (ui_welcome_popup.c)
- 手动构建:全屏 overlay + 卡片
- `lv_obj_create(scr)` → `lv_obj_add_flag(s_popup, LV_OBJ_FLAG_FLOATING)`
- 全屏 20% 黑色遮罩:`lv_obj_set_style_bg_opa(s_popup, LV_OPA_20, 0)`
- 卡片 360x168,圆角 10,白色背景
- 关闭按钮:X 按钮 + `lv_obj_delete(s_popup)`
- 触发:`UI_EVENT_SCENE_CHANGED` → 检查 `current_scene == IOT_SCENE_HOME`

### ui_kit.c 通用 popup
- `ui_kit_popup_create(parent, title, accent_color)` 工厂函数
- 同样模式:overlay + card + close_btn
- `LV_OBJ_FLAG_FLOATING` + `lv_pct(100)` 全屏

### lv_msgbox_create 使用方式
- `lv_msgbox_create(NULL)` - parent=NULL 创建模态 msgbox,自动带背景遮罩(backdrop)
- 这是 LVGL 内置的模态对话框,自动拦截背景点击
- 但火警弹窗没有用 `LV_OBJ_FLAG_FLOATING`,因为 msgbox 自带模态特性

## 4. 输入拦截

### LVGL 拦截所有点击的方法
1. **`lv_msgbox_create(NULL)`**:自动创建模态,带背景遮罩,拦截所有背景点击
2. **`LV_OBJ_FLAG_FLOATING` + 全屏覆盖**:
   - `lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING)` - 使对象脱离正常布局,z-order 最顶
   - 配合 `lv_obj_set_size(obj, lv_pct(100), lv_pct(100))` 全屏
   - 配合 `lv_obj_set_style_bg_opa(obj, LV_OPA_20, 0)` 半透明遮罩
   - 这样可以拦截侧栏点击

### 关键点
- 侧栏 `side_bar_` 是 `lv_screen_active()` 的直接子对象
- 智能家居页 `smart_home_page_` 也是 `lv_screen_active()` 的直接子对象
- 要拦截侧栏,弹窗必须挂在 `lv_screen_active()` 上,且使用 `LV_OBJ_FLAG_FLOATING`
- 火警弹窗用 `lv_msgbox_create(NULL)` 自动挂到 active screen 的 layer top
- 欢迎弹窗手动挂到 `lv_screen_active()` + `LV_OBJ_FLAG_FLOATING`

### "登录前不能操作任何 UI" 实现
- 创建全屏 floating overlay 覆盖整个屏幕(包括侧栏区域)
- overlay 设置 `LV_OBJ_FLAG_CLICKABLE` 拦截点击事件
- 在 overlay 上放置登录表单
- 登录成功后 `lv_obj_delete(overlay)`

## 5. 状态持久化

### NVS 使用方式 (settings.h / settings.cc)
- `Settings` 类封装 NVS 操作
- 构造:`Settings("namespace", read_write)` - `nvs_open()`
- 析构:`nvs_close()` + 可选 `nvs_commit()`
- API:`GetString/SetString/GetInt/SetInt/GetBool/SetBool/EraseKey/EraseAll`
- 已有命名空间:"display"(主题)、"assets"(下载URL)、"wifi"(SSID)

### SsidManager 参考
- 在 `wifi_board.cc` 中使用:`SsidManager::GetInstance().GetSsidList()`
- 来自外部库 `<ssid_manager.h>`(可能在 components 中)
- 单例模式,管理 SSID 列表

### 登录状态持久化方案
- 可用 `Settings("auth", true)` 命名空间
- `SetBool("logged_in", true)` / `GetBool("logged_in", false)`
- 或存储 token: `SetString("token", "...")` / `GetString("token", "")`

### 是否每次上电都登录
- 取决于需求,有两种方案:
  1. **持久化登录**:NVS 存 `logged_in=true`,上电直接进入(除非 token 过期)
  2. **每次上电登录**:不持久化或每次校验 token 有效性
- SsidManager 模式:SSID 持久化,上电自动重连(类似方案1)

## 6. 现有"模式"概念

### DeviceState 状态机 (device_state.h)
```c
enum DeviceState {
    kDeviceStateUnknown,        // 未知
    kDeviceStateStarting,       // 启动中
    kDeviceStateWifiConfiguring,// WiFi 配置中
    kDeviceStateIdle,           // 空闲
    kDeviceStateConnecting,     // 连接中
    kDeviceStateListening,      // 监听中
    kDeviceStateSpeaking,       // 说话中
    kDeviceStateUpgrading,      // 升级中
    kDeviceStateActivating,     // 激活中
    kDeviceStateAudioTesting,   // 音频测试
    kDeviceStateFatalError      // 致命错误
};
```

### 状态转换规则 (device_state_machine.cc:34-102)
- `kDeviceStateUnknown` → `kDeviceStateStarting`
- `kDeviceStateStarting` → `kDeviceStateWifiConfiguring` / `kDeviceStateActivating`
- `kDeviceStateWifiConfiguring` → `kDeviceStateActivating` / `kDeviceStateAudioTesting`
- `kDeviceStateIdle` → 多种状态
- `kDeviceStateFatalError` → 不可转出

### 状态变化通知机制
- `state_machine_.AddStateChangeListener(callback)` - 注册监听器
- `TransitionTo()` 时调用 `NotifyStateChange()` → 回调
- Application 中:回调设置 `MAIN_EVENT_STATE_CHANGED` 事件位
- 主循环 `HandleStateChangedEvent()` 处理状态变化(更新 UI、音频等)

### 扩展 kDeviceStateLocked 的可行性
- **可以扩展**:在 `device_state.h` 添加 `kDeviceStateLocked`
- 需要在 `device_state_machine.cc` 添加转换规则:
  - `kDeviceStateStarting` → `kDeviceStateLocked`(启动后进入锁屏)
  - `kDeviceStateLocked` → `kDeviceStateIdle`(登录成功后)
- 需要更新 `STATE_STRINGS[]` 数组
- 需要在 `HandleStateChangedEvent()` 添加 case 处理

### 状态变化时 UI 响应
- `HandleStateChangedEvent()` (application.cc:875-947)
- 根据 new_state 调用 `display->SetStatus()`, `SetEmotion()`, `SetChatMessage()`
- 控制 `audio_service_` 的语音处理/唤醒词检测
- 可在此添加 `kDeviceStateLocked` case:显示登录弹窗,禁用音频

## 可扩展点总结

### 方案A:基于 DeviceState 扩展
1. 在 `device_state.h` 添加 `kDeviceStateLocked`
2. 在 `device_state_machine.cc` 添加转换规则
3. 在 `Application::Initialize()` 中 `SetupUI()` 后检查 NVS 登录状态
4. 未登录 → `SetDeviceState(kDeviceStateLocked)` → 显示登录弹窗
5. 登录成功 → `SetDeviceState(kDeviceStateIdle)`

### 方案B:纯 UI 层弹窗(不改状态机)
1. 在 `lcd_display.cc::SetupUI()` 末尾或 `CreateSmartHomeShell()` 后添加登录检查
2. 创建全屏 floating overlay 拦截所有交互
3. 参考 `ui_welcome_popup.c` 或 `ui_kit.c::ui_kit_popup_create` 模式
4. 登录成功后 `lv_obj_delete(overlay)`

### 推荐插入点
- **最佳位置**:`Application::Initialize()` 中 `display->SetupUI()` 之后(line 70 后)
  - 此时 LVGL 已就绪,侧栏和容器已创建
  - 此时 SmartHomeTasksStart() 还未调用,可以决定是否启动
  - 此时 network_start 任务还未创建,可以决定是否联网
- **UI 创建位置**:`CreateSmartHomeShell()` 末尾(line 772 前)
  - 此时侧栏、smart_home_page_ 都已创建
  - 可以在 `lv_screen_active()` 上叠加登录 overlay

### 弹窗实现建议
- 参考 `ui_welcome_popup.c` 的手动构建方式(更灵活)
- 或参考 `smart_home_alarm_ui.c` 的 `lv_msgbox_create(NULL)`(更简单,自动模态)
- 关键:`LV_OBJ_FLAG_FLOATING` + 全屏 + `LV_OBJ_FLAG_CLICKABLE`

Now let me write up the comprehensive analysis.

The user wants me to do a deep research on the xiaozhi-for-p4 project's MCP image recognition chain and esp-face integration feasibility. Let me start by exploring the codebase to understand the current implementation.

Let me plan my searches:
1. MCP server implementation (mcp_server.cc)
2. EspVideo capture and explain functions
3. LCD display preview image
4. idf_component.yml for dependencies
5. Partition table for assets
6. Camera frame format
7. MCP protocol schema

Let me do parallel searches to be efficient.

Now let me search for the lcd_display.cc SetPreviewImage implementation, the camera.h interface, and the partition table.

Now let me look at the partition table, the lvgl_image.h, the lcd_display.h to understand the preview image, and search for any P4 board-specific code.

Now let me look at the lcd_display.cc around the preview image setup, and also search for how the Application::SendMcpMessage and how the chat flow works. Also need to look at the application.cc for how the MCP messages flow.

Now let me look at the lcd_display.cc around the preview image setup (the area around line 1420 is the second SetPreviewImage, which is for the Smart Home Shell). Let me also look at the lcd_display.cc SetupUI to understand the preview_image_ object placement.

Now let me look at the Kconfig to see what camera options are available, and also check the sdkconfig to see what's enabled.

Now let me search for the esp-face component, look at the smart_home MCP tool, and look at the application.cc to understand the chat flow.

Now I have a good understanding of the MCP image recognition chain. Let me also check the esp-sr component (which is already in the project) and look for any existing face recognition related code. Let me also check the CMakeLists.txt to understand the build structure.

Now let me look at the CMakeLists.txt to understand the build setup, and also check if there's any existing face recognition code. Let me also look at the application.cc to understand how TTS is handled.

Now I have enough information to provide a comprehensive analysis. Let me also check the esp-sr README for face recognition info, and look at the Kconfig for the camera settings.

Actually, I have enough information now. Let me compile the comprehensive analysis. The user wants:

1. Current MCP image recognition chain analysis
2. Camera preview analysis
3. esp-face / ESP-WHO component analysis
4. Face recognition flow analysis
5. Model storage solution analysis
6. Camera frame format analysis
7. Xiaozhi MCP protocol analysis

Let me also briefly look at the lcd_display.cc around line 613 to understand the shell layout for preview image.

基于对 xiaozhi-for-p4 项目的深度调研,以下是完整的分析报告。

---

# xiaozhi-for-p4 MCP 识图链路与 esp-face 集成可行性调研报告

## 1. 当前 MCP 识图链路完整分析

### 1.1 take_photo 工具注册

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.cc:100-122`

```cpp
auto camera = board.GetCamera();
if (camera) {
    AddTool("self.camera.take_photo",
        "Always remember you have a camera. If the user asks you to see something, use this tool to take a photo and then explain it.\n"
        "Args:\n"
        "  `question`: The question that you want to ask about the photo.\n"
        "Return:\n"
        "  A JSON object that provides the photo information.",
        PropertyList({
            Property("question", kPropertyTypeString)
        }),
        [camera](const PropertyList& properties) -> ReturnValue {
            TaskPriorityReset priority_reset(1);  // 降低优先级以进行摄像头捕获
            if (!camera->Capture()) {
                throw std::runtime_error("Failed to capture photo");
            }
            auto question = properties["question"].value<std::string>();
            return camera->Explain(question);
        });
}
```

**关键点**:
- 工具名: `self.camera.take_photo`
- 输入: `question` (字符串,必填)
- 输出: 字符串(JSON 格式的 AI 识别结果)
- 在 `AddCommonTools()` 中注册,只在 `board.GetCamera()` 返回非空时添加
- 调用时降低任务优先级(`TaskPriorityReset(1)`),避免阻塞主线程

### 1.2 Camera 抽象接口

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\boards\common\camera.h`

```cpp
class Camera {
public:
    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }
    virtual std::string Explain(const std::string& question) = 0;
};
```

P4 板使用 `EspVideo` 实现(在 `main/boards/common/esp_video.cc`),S3 板使用 `Esp32Camera` 实现。

### 1.3 EspVideo::Capture() 函数

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\boards\common\esp_video.cc:388-841`

**核心流程**:
1. **等待编码线程完成** (line 389-391): `encoder_thread_.join()`
2. **丢弃前 2 帧** (line 397-405): 循环 3 次 DQBUF,前 2 次直接 QBUF 丢弃(曝光稳定)
3. **保存第 3 帧到 PSRAM** (line 405-492):
   - `heap_caps_malloc(frame_.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` 分配 PSRAM
   - 根据传感器格式处理:
     - RGB565/RGB24/YUYV/YUV420/GREY/JPEG: 直接 memcpy
     - YUV422P: 实际是 YUYV,标记为 V4L2_PIX_FMT_YUYV
     - RGB565X: 大端转小端
4. **可选旋转** (line 494-724, 由 `CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE` 控制):
   - P4 使用 PPA 硬件加速(`ppa_do_scale_rotate_mirror`)
   - 其他芯片使用软件 `esp_imgfx_rotate`
5. **显示预览图** (line 732-839):
   - 调用 `display->SetPreviewImage()`
   - YUV/RGB24 转 RGB565(使用 `esp_imgfx_color_convert`)
   - JPEG 输入用 `jpeg_to_image` 解码

### 1.4 EspVideo::Explain() 函数

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\boards\common\esp_video.cc:900-1041`

**核心流程**:
1. **创建 JPEG 编码队列** (line 906-910): `xQueueCreate(40, sizeof(JpegChunk))`
2. **启动编码线程** (line 913-942):
   - 调用 `image_to_jpeg_cb()` 异步编码 JPEG(质量 80)
   - 通过队列分块传输(每块独立分配 PSRAM)
3. **HTTP POST 上传** (line 944-1026):
   - 使用 `Board::GetInstance().GetNetwork()->CreateHttp(3)` (HTTP 通道 id=3)
   - **multipart/form-data** 格式,分块传输编码(`Transfer-Encoding: chunked`)
   - 字段:
     - `question`: 用户问题
     - `file`: JPEG 文件(camera.jpg, image/jpeg)
   - Headers:
     - `Device-Id`: MAC 地址
     - `Client-Id`: UUID
     - `Authorization`: `Bearer <token>` (如果 token 非空)
4. **返回 AI 识别结果** (line 1033): `http->ReadAll()` 读取服务器响应

### 1.5 explain_url 配置来源

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.cc:334-351`

```cpp
void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}
```

**配置流程**:
1. 设备启动后通过 OTA 激活,小智服务器返回 MQTT/WebSocket endpoint
2. 设备连接 MQTT/WebSocket 后,服务器发送 `initialize` MCP 消息
3. `initialize` 消息的 `params.capabilities.vision.url` 和 `vision.token` 被解析
4. 调用 `camera->SetExplainUrl(url, token)` 保存到 `EspVideo::explain_url_` 和 `explain_token_` 成员变量
5. **不存储在 NVS 或 Kconfig**,每次启动都需要重新下发

### 1.6 TTS 播报链路

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\application.cc:541-569`

```cpp
protocol_->OnIncomingJson([this, display](const cJSON* root) {
    auto type = cJSON_GetObjectItem(root, "type");
    if (strcmp(type->valuestring, "tts") == 0) {
        auto state = cJSON_GetObjectItem(root, "state");
        if (strcmp(state->valuestring, "start") == 0) {
            Schedule([this]() {
                aborted_ = false;
                SetDeviceState(kDeviceStateSpeaking);
            });
        } else if (strcmp(state->valuestring, "sentence_start") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                Schedule([display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("assistant", message.c_str());
                });
            }
        }
    }
});
```

**完整链路**:
1. 用户语音"你看到了什么" → AFE 唤醒 → STT 转文字
2. 服务器 LLM 决策调用 `self.camera.take_photo` 工具
3. 设备通过 MCP `tools/call` 收到调用请求
4. `McpServer::DoToolCall` 在主线程执行:
   - `camera->Capture()` 拍照 + 显示预览
   - `camera->Explain(question)` 上传 JPEG,返回 AI 描述字符串
5. `McpServer::ReplyResult` 通过 `Application::SendMcpMessage` → `protocol_->SendMcpMessage` 把结果发回服务器
6. 服务器 LLM 收到工具结果,生成自然语言回复
7. 服务器下发 `tts` 消息,设备播放音频 + 显示文字

**注意**: AI 描述不是本地 TTS 播报,而是返回给云端 LLM,由 LLM 整合后再下发 TTS。

---

## 2. 摄像头预览分析

### 2.1 SetPreviewImage 调用点

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\boards\common\esp_video.cc:732-839`

```cpp
// 显示预览图片
auto display = dynamic_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
if (display != nullptr) {
    // ...格式转换...
    auto image = std::make_unique<LvglAllocatedImage>(data, lvgl_image_size, w, h, stride, color_format);
    display->SetPreviewImage(std::move(image));
}
```

### 2.2 LcdDisplay::SetPreviewImage 实现

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\display\lcd_display.cc:1420-1454`

```cpp
void LcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image is not initialized");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (gif_controller_) {
            gif_controller_->Start();
        }
        return;
    }

    preview_image_cached_ = std::move(image);
    auto img_dsc = preview_image_cached_->image_dsc();
    lv_image_set_src(preview_image_, img_dsc);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // zoom factor 0.5
        lv_image_set_scale(preview_image_, 128 * width_ / img_dsc->header.w);
    }

    // Hide emoji_box_
    if (gif_controller_) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}
```

### 2.3 预览图 UI 位置和尺寸

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\display\lcd_display.cc:1270-1274`

```cpp
/* Middle layer: preview_image_ - centered display */
preview_image_ = lv_image_create(screen);
lv_obj_set_size(preview_image_, width_ / 2, height_ / 2);  // 512x300 (P4 1024x600)
lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
```

**Shell 布局调整** (`lcd_display.cc:616-618`):
```cpp
if (preview_image_) {
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, kShellSidebarWidth / 2, 0);
}
```
- `kShellSidebarWidth = 128` (line 30)
- 在 Shell 模式下,预览图相对屏幕中心右移 64 像素,避开 128px 侧栏

**显示特性**:
- 缩放因子: `128 * width_ / img_dsc->header.w` (即缩放到屏幕一半宽度)
- 自动隐藏 emoji_box_,显示 preview_image_
- 5 秒后自动隐藏(`PREVIEW_IMAGE_DURATION_MS = 5000`,定义在 `lcd_display.h:14`)
- 通过 `esp_timer_start_once` 单次定时器自动恢复

### 2.4 复用为"人脸识别预览"可行性

**完全可行**,只需:
1. 直接调用 `display->SetPreviewImage(std::move(image))` 即可显示任意 RGB565 图像
2. 已有完整的生命周期管理(5 秒自动隐藏、内存自动释放)
3. `LvglAllocatedImage` 析构时通过 `LV_EVENT_DELETE` 回调释放 `heap_caps_free`
4. 人脸识别时可在每帧检测后调用 `SetPreviewImage` 刷新预览(注意频率,建议 5-10 FPS)

**潜在问题**:
- 当前 `preview_image_cached_` 是 unique_ptr,每次 SetPreviewImage 会替换上一帧(适合静态预览,不适合视频流)
- 5 秒定时器会强制隐藏,人脸识别期间需要禁用或重置定时器
- LVGL 锁是 `DisplayLockGuard`,高频调用可能阻塞 LVGL 任务

---

## 3. esp-face / ESP-WHO 组件集成

### 3.1 当前状态

**未集成**。在 `e:\MCU\esp32\p4\xiaozhi-for-p4\main\idf_component.yml` 中没有 `esp-face` 依赖,`managed_components/` 下也没有 `espressif__esp-face` 目录。当前已集成的相关组件:
- `espressif/esp-sr: ~2.3.0` (语音识别,已包含 dl_lib 通用深度学习库)
- `espressif/esp_image_effects: ^1.0.1` (图像旋转/颜色转换)
- `espressif/esp_video: ==1.3.1` (V4L2 摄像头抽象)
- `espressif/esp_new_jpeg: ^0.6.1` (JPEG 编解码)

### 3.2 idf_component.yml 添加方式

在 `e:\MCU\esp32\p4\xiaozhi-for-p4\main\idf_component.yml` 添加:

```yaml
espressif/esp-face:
  version: '*'
  rules:
  - if: target in [esp32p4, esp32s3]
```

**注意**: esp-face 是 ESP-WHO 项目的核心组件,从 ESP-WHO v2.0 起拆分为独立组件。最新版本(截至 2025 年)支持 ESP32-P4。

### 3.3 esp-face API 接口

esp-face 主要提供以下 API(基于 ESP-WHO 文档):

**人脸检测 (MTMN)**:
```c
// 检测配置
mtmn_config_t mtmn_config = mtmn_face_config();
// 配置参数:
//   min_face_size: 最小人脸尺寸(像素)
//   pyramid: 金字塔层数
//   threshold: 检测阈值(det_score_thresh, det_threshold)

// 检测人脸
box_array_t *net_boxes = face_detect(image, &mtmn_config);
// 返回 box_array_t:
//   box: 人脸矩形框 [x1,y1,x2,y2]
//   score: 置信度
//   landmark: 5 个关键点(左眼、右眼、鼻、左嘴、右嘴)
```

**人脸对齐**:
```c
// 根据关键点对齐人脸(112x112 输入 MFN)
dl_matrix3du_t *aligned_face = face_align(image, net_boxes->landmark, 0);
```

**特征提取 (MFN - Mobile FaceNet)**:
```c
// 加载模型
mfnc_t *mfnc = mfnc_load_from_flash(model_data, model_size);

// 提取 256 维特征
dl_matrix3du_t *face_id = mfnc_feature_extraction(mfnc, aligned_face);
// face_id->item 是 256 维浮点向量
```

**人脸比对**:
```c
// 余弦相似度
float similarity = cos_distance(face_id1, face_id2);
// 阈值: 0.5-0.7 (典型 0.55)
```

### 3.4 ESP32-P4 支持情况

**支持**。esp-face 已针对 ESP32-P4 优化:
- **PIE 向量指令**: P4 的 HP CPU 支持 PIE (P4 Instruction Extension),esp-face 的 dl_lib 已针对 PIE 优化
- **PSRAM**: P4 通常配 32MB PSRAM,足够加载模型和帧缓冲
- **性能**: MTMN 检测约 200-400ms (640x480 输入),MFN 特征提取约 100-200ms
- **参考**: `managed_components/espressif__esp-sr/lib/esp32p4/` 已有 esp-sr 的 P4 库,dl_lib 是共享的

### 3.5 模型文件加载

esp-face 模型通常打包为 C 数组头文件或二进制文件:

**方式 1: 头文件嵌入(默认)**
```c
#include "face_detect.h"      // MTMN 模型
#include "face_recognition.h"  // MFN 模型
// 模型作为 const unsigned char[] 数组编译进固件
```

**方式 2: 从 Flash/SPIFFS 加载**
```c
// 从文件读取模型
FILE *f = fopen("/spiffs/mfn_model.bin", "rb");
fseek(f, 0, SEEK_END);
size_t size = ftell(f);
fseek(f, 0, SEEK_SET);
uint8_t *model_data = malloc(size);
fread(model_data, 1, size, f);
fclose(f);
mfnc_t *mfnc = mfnc_load_from_flash(model_data, size);
```

### 3.6 人脸库存储

**特征向量存储方案**:
- 每人 256 维 float = 1024 字节
- 10 人 = 10KB,100 人 = 100KB

**推荐方案**: NVS (Non-Volatile Storage)
- 当前 `nvs` 分区 16KB (`partitions/v2/16m.csv:3`),可存几十个人脸
- 优点: 原生支持,无需额外文件系统
- 缺点: 16KB 限制,大量人脸需扩大 NVS 或用 SPIFFS

**备选方案**: SPIFFS assets 分区
- 当前 assets 分区 8MB,空间充足
- 需要自定义文件格式(如 JSON + 二进制特征)
- 可复用 `Assets::GetAssetData()` 机制

---

## 4. 人脸识别流程

### 4.1 检测 (MTMN)

```
输入帧 (RGB565/RGB888)
    ↓
图像金字塔 (多尺度)
    ↓
P-Net (Proposal Network) 候选框
    ↓
R-Net (Refinement Network) 筛选
    ↓
O-Net (Output Network) 输出框 + 关键点
    ↓
box_array_t:
  - box: [x1,y1,x2,y2] (人脸矩形)
  - score: 置信度 (0-1)
  - landmark: 5x2 关键点 (左眼、右眼、鼻、左嘴、右嘴)
```

### 4.2 对齐

根据 5 个关键点进行仿射变换,输出 112x112 对齐人脸图像(MFN 输入尺寸)。

### 4.3 特征提取 (MFN)

```
对齐人脸 (112x112 RGB)
    ↓
MFN (Mobile FaceNet) 卷积网络
    ↓
256 维特征向量 (L2 归一化)
```

### 4.4 比对

```c
float cos_distance(dl_matrix3du_t *id1, dl_matrix3du_t *id2) {
    float dot = 0, norm1 = 0, norm2 = 0;
    for (int i = 0; i < 256; i++) {
        dot += id1->item[i] * id2->item[i];
        norm1 += id1->item[i] * id1->item[i];
        norm2 += id2->item[i] * id2->item[i];
    }
    return dot / (sqrt(norm1) * sqrt(norm2));
}
```

**阈值建议**:
- `> 0.7`: 高置信度匹配
- `0.55-0.7`: 正常匹配(典型阈值 0.55)
- `< 0.55`: 不匹配

### 4.5 注册流程

1. 用户触发注册(语音"录入人脸"或 UI 按钮)
2. 调用 `Capture()` 获取一帧
3. MTMN 检测人脸,若无人脸提示重试
4. 对齐 + MFN 提取特征
5. 保存到人脸库:
   - 用户 ID (字符串)
   - 用户名 (字符串)
   - 256 维特征向量 (1024 字节)
6. 持久化到 NVS 或 SPIFFS

---

## 5. 模型存储方案

### 5.1 assets 分区当前使用情况

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\partitions\v2\16m.csv`

```
nvs,      data, nvs,     0x9000,    0x4000,        # 16KB
otadata,  data, ota,     0xd000,    0x2000,        # 8KB
phy_init, data, phy,     0xf000,    0x1000,        # 4KB
ota_0,    app,  ota_0,   0x20000,   0x3f0000,      # ~4MB
ota_1,    app,  ota_1,   ,          0x3f0000,      # ~4MB
assets,   data, spiffs,  0x800000,  8M              # 8MB
```

**当前 assets 用途** (见 `e:\MCU\esp32\p4\xiaozhi-for-p4\main\assets.cc` 和 `partitions/v2/README.md`):
- 唤醒词模型 (srmodels.bin)
- 字体文件 (fonts.bin)
- Emoji 集合
- 主题文件(背景图、颜色配置)
- 语言配置

**加载机制** (`assets.cc:130-185`):
- `esp_partition_mmap` 整个分区到内存
- 自定义二进制格式: 12 字节头 + 文件表 + 文件数据
- 头部: `stored_files (4B) + checksum (4B) + stored_len (4B)`
- 文件表项: `name[32] + size(4B) + offset(4B) + width(2B) + height(2B)`
- 每个文件以 "ZZ" 魔数开头
- 通过 `Assets::GetAssetData(name, ptr, size)` 访问

### 5.2 模型文件打包到 assets

**可行但需谨慎**。esp-face 模型通常较大:
- MTMN 三阶段模型: ~500KB-1MB
- MFN 模型: ~2-4MB
- 总计: ~3-5MB,8MB assets 分区可容纳

**打包方式**:
1. 将模型二进制文件加入 assets 打包脚本
2. 在 `index.json` 添加模型条目
3. 运行时通过 `Assets::GetAssetData("mfn_model.bin", ptr, size)` 获取指针
4. 调用 `mfnc_load_from_flash(ptr, size)` 加载

**注意**: assets 分区是 mmap 的,模型直接在 Flash 上执行(XIP),不占用 PSRAM。但 PIE 指令可能需要模型在 PSRAM 中,需测试验证。

### 5.3 运行时加载到 PSRAM

```c
// 从 assets 获取模型指针(Flash mmap)
void *model_ptr = nullptr;
size_t model_size = 0;
Assets::GetInstance().GetAssetData("mfn_model.bin", model_ptr, model_size);

// 复制到 PSRAM(如果需要)
uint8_t *psram_model = (uint8_t*)heap_caps_malloc(model_size, MALLOC_CAP_SPIRAM);
memcpy(psram_model, model_ptr, model_size);

// 加载模型
mfnc_t *mfnc = mfnc_load_from_flash(psram_model, model_size);
```

### 5.4 人脸库存储建议

**方案 A: NVS (推荐,小规模)**
- 当前 NVS 16KB,扣除其他数据约剩 10KB
- 可存 ~10 个人脸(每人 1024B 特征 + 100B 元数据)
- 优点: 简单、原子写入
- 缺点: 容量有限

**方案 B: SPIFFS assets 分区 (推荐,大规模)**
- 复用现有 8MB assets 分区
- 自定义文件: `face_db.json` (元数据) + `face_features.bin` (特征向量)
- 优点: 容量大、可 OTA 更新
- 缺点: 需要实现读写逻辑(assets 当前是只读 mmap)

**方案 C: 独立 face_db 分区**
- 修改分区表,从 assets 划出 1MB 作为 `face_db` 分区
- 优点: 隔离、独立管理
- 缺点: 需要修改分区表,影响 OTA

---

## 6. 摄像头帧格式

### 6.1 当前支持的格式

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\boards\common\esp_video.cc:194-235`

```cpp
// 旋转模式下的格式优先级
auto get_rank = [](uint32_t fmt) -> int {
    switch (fmt) {
        case V4L2_PIX_FMT_RGB24:    return 0;  // 最优
        case V4L2_PIX_FMT_RGB565:   return 1;
        case V4L2_PIX_FMT_YUV420:   return 2;  // 仅硬件 JPEG 编码时
        case V4L2_PIX_FMT_GREY:
        case V4L2_PIX_FMT_YUV422P:
        default:                     return 1 << 29;  // 不支持
    }
};

// 非旋转模式
auto get_rank = [](uint32_t fmt) -> int {
    switch (fmt) {
        case V4L2_PIX_FMT_YUV422P:  return 10;  // 实际是 YUYV
        case V4L2_PIX_FMT_RGB565:   return 11;
        case V4L2_PIX_FMT_RGB24:    return 12;
        case V4L2_PIX_FMT_YUV420:   return 13;
        case V4L2_PIX_FMT_JPEG:     return 5;   // USB 摄像头
        case V4L2_PIX_FMT_GREY:     return 20;
        default:                     return 1 << 29;
    }
};
```

**当前 sdkconfig 配置** (`sdkconfig:3027-3051`):
- `CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE=y`
- `CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE=y`
- `CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE=y` (P4 PIE 字节序交换)
- `CONFIG_ESP_VIDEO_ENABLE_SWAP_SHORT=y` (P4 PIE 短字序交换)
- `CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE` 未启用(默认 n)
- `CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT` 未启用

**P4 EV Board 实际输出格式**:
- MIPI-CSI 传感器(如 SC200AI 或 OV5647)输出 RGB565 或 YUYV
- 经 ISP 处理后输出 RGB565(默认)
- `frame_.format` 在 `Capture()` 中保存,用于后续处理

### 6.2 人脸识别所需格式

**esp-face 输入格式**:
- MTMN 检测: RGB565 或 RGB888(取决于实现)
- MFN 识别: RGB888 (112x112)

**当前 `Capture()` 输出**:
- `frame_.format`: V4L2_PIX_FMT_RGB565 或 V4L2_PIX_FMT_YUYV
- `frame_.data`: PSRAM 中的原始帧数据

### 6.3 格式转换开销

**当前已有的转换** (`esp_video.cc:746-835`):
- YUYV/YUV420/RGB24 → RGB565 (用于 LVGL 预览)
- 使用 `esp_imgfx_color_convert` 软件 API
- 640x480 转换约 30-50ms

**人脸识别需要**:
- RGB565 → RGB888: 简单位扩展,~10ms (640x480)
- 或直接配置传感器输出 RGB888(如果支持)

### 6.4 PPA 硬件加速

**当前已用 PPA** (`esp_video.cc:574-723`):
- 仅用于旋转(`ppa_do_scale_rotate_mirror`)
- 支持 RGB565 和 RGB888 输入
- YUYV 需先软件转 RGB888 再用 PPA

**PPA 能力** (`main/smart_home/ui/bsp_ppa.c`):
- `PPA_OPERATION_SRM`: 缩放/旋转/镜像
- `PPA_OPERATION_FILL`: 矩形填充
- 不支持颜色空间转换(YUV→RGB)

**结论**: PPA 不能用于 YUV→RGB 转换,但可用于人脸对齐时的仿射变换(如果支持)。颜色转换仍需软件 `esp_imgfx_color_convert`。

---

## 7. 小智 MCP 协议

### 7.1 协议规范

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.cc:4` (注释)

参考: https://modelcontextprotocol.io/specification/2024-11-05

**传输层** (`main/protocols/protocol.cc:76-79`):
```cpp
void Protocol::SendMcpMessage(const std::string& payload) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"mcp\",\"payload\":" + payload + "}";
    SendText(message);
}
```

MCP 消息封装在小智的 `type:"mcp"` 消息的 `payload` 字段中。

### 7.2 JSON-RPC 2.0 消息格式

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.cc:353-436`

**initialize 请求** (服务器→设备):
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "capabilities": {
      "vision": {
        "url": "https://api.example.com/explain",
        "token": "Bearer token"
      }
    }
  }
}
```

**initialize 响应** (设备→服务器, `mcp_server.cc:394-398`):
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {"tools": {}},
    "serverInfo": {
      "name": "ESP32P4FuncEV",
      "version": "1.0.0"
    }
  }
}
```

**tools/list 请求**:
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {
    "cursor": "",
    "withUserTools": false
  }
}
```

**tools/list 响应** (`mcp_server.cc:455-509`):
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "self.camera.take_photo",
        "description": "Always remember you have a camera...",
        "inputSchema": {
          "type": "object",
          "properties": {
            "question": {"type": "string"}
          },
          "required": ["question"]
        }
      }
    ],
    "nextCursor": "self.iot.set_servo_by_index"
  }
}
```

**分页**: 单次响应最大 8000 字节(`max_payload_size = 8000`, line 456),超出用 `nextCursor` 分页。

### 7.3 take_photo 工具 schema

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.h:208-312` (McpTool::to_json)

```json
{
  "name": "self.camera.take_photo",
  "description": "Always remember you have a camera. If the user asks you to see something, use this tool to take a photo and then explain it.\nArgs:\n  `question`: The question that you want to ask about the photo.\nReturn:\n  A JSON object that provides the photo information.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "question": {"type": "string"}
    },
    "required": ["question"]
  }
}
```

**tools/call 请求** (服务器→设备):
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "self.camera.take_photo",
    "arguments": {
      "question": "What do you see?"
    }
  }
}
```

**tools/call 响应** (`mcp_server.h:272-311`, McpTool::Call):
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"success\":true,\"result\":\"I see a person sitting at a desk...\"}"
      }
    ],
    "isError": false
  }
}
```

### 7.4 扩展新工具(如 face_login)

**位置参考**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\smart_home\mcp\smart_home_mcp_tool.cc:104-263`

**注册方式**:
```cpp
extern "C" void SmartHomeMcp_RegisterTools(void) {
    auto& server = McpServer::GetInstance();
    
    server.AddTool("self.face.login",
        "Perform face login. Capture a photo and match against registered faces.\n"
        "Returns the user name if matched, or 'unknown' if no match.",
        PropertyList({
            Property("timeout_ms", kPropertyTypeInteger, 5000, 1000, 10000)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            int timeout = properties["timeout_ms"].value<int>();
            // 1. 调用 EspVideo::Capture() 获取帧
            // 2. MTMN 检测人脸
            // 3. MFN 提取特征
            // 4. 与人脸库比对
            // 5. 返回匹配的用户名
            return std::string("user_001");
        });
    
    server.AddTool("self.face.register",
        "Register a new face. Capture a photo and save the face feature.",
        PropertyList({
            Property("user_name", kPropertyTypeString)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            auto name = properties["user_name"].value<std::string>();
            // 注册流程
            return true;
        });
}
```

**调用点**: 在板级初始化时调用,例如在 `ESP32P4FunctionEvBoard` 构造函数或 `Application::Initialize()` 中。

### 7.5 LLM 如何知道何时调用 take_photo

**机制**: 工具描述中的自然语言提示。

`mcp_server.cc:102-107` 的描述:
```
"Always remember you have a camera. If the user asks you to see something, use this tool to take a photo and then explain it."
```

LLM 在收到 `tools/list` 响应后,将所有工具的 `description` 作为系统提示的一部分。当用户说"你看到了什么"时,LLM 根据 description 匹配到 `self.camera.take_photo` 工具,然后发起 `tools/call`。

**关键设计**:
- 工具描述要明确说明"何时使用"
- `AddCommonTools()` 中的工具放在列表前面,利用 prompt cache 加速
- 工具描述用英文(兼容多语言 LLM)

### 7.6 user_only 工具

**位置**: `e:\MCU\esp32\p4\xiaozhi-for-p4\main\mcp_server.cc:128-301`

`AddUserOnlyTool` 注册的工具对 LLM 不可见(`annotations.audience = ["user"]`),只在 `tools/list` 请求带 `withUserTools: true` 时返回。当前 user_only 工具:
- `self.get_system_info`
- `self.reboot`
- `self.upgrade_firmware`
- `self.screen.get_info`
- `self.screen.snapshot`
- `self.screen.preview_image`
- `self.assets.set_download_url`

**人脸注册建议**: `self.face.register` 应该是 user_only(避免 LLM 自动注册),`self.face.login` 可以开放给 LLM。

---

## 8. 可行性总结

### 8.1 集成 esp-face 可行性: 高

**优势**:
- ESP32-P4 已被 esp-face 官方支持(PIE 指令优化)
- 32MB PSRAM 充足(模型 + 帧缓冲 + 特征库)
- 已有 `EspVideo::Capture()` 提供原始帧
- 已有 `SetPreviewImage()` 可复用为人脸预览
- assets 分区 8MB 可存储模型
- MCP 工具扩展机制成熟(`AddTool` + lambda)

**挑战**:
- esp-face 与 esp-sr 的 dl_lib 版本可能冲突(需测试)
- 模型文件需要打包到 assets 或单独分区
- 人脸库持久化需要新机制(NVS 或 SPIFFS 读写)
- 实时人脸检测(视频流)需要优化 Capture() 为非阻塞
- 当前 `Capture()` 丢弃前 2 帧(曝光稳定),人脸识别可能需要更多帧

### 8.2 推荐实施路径

1. **Phase 1: 静态识别**
   - 添加 `espressif/esp-face` 依赖
   - 实现 `self.face.login` MCP 工具(复用 `Capture()` 单帧)
   - 模型打包到 assets
   - 人脸库存 NVS(小规模)

2. **Phase 2: 注册流程**
   - 实现 `self.face.register` user_only 工具
   - UI 页面显示注册状态(复用 `SetPreviewImage`)

3. **Phase 3: 实时检测(可选)**
   - 修改 `EspVideo` 支持连续帧回调
   - 后台任务持续检测,检测到人脸时触发事件
   - 与智能家居联动(如"主人回家自动开灯")

### 8.3 关键代码位置速查

| 功能 | 文件 | 行号 |
|------|------|------|
| MCP 工具注册 | `main/mcp_server.cc` | 100-122 |
| take_photo 实现 | `main/mcp_server.cc` | 111-120 |
| explain_url 配置 | `main/mcp_server.cc` | 334-351 |
| Camera 抽象 | `main/boards/common/camera.h` | 6-14 |
| EspVideo::Capture | `main/boards/common/esp_video.cc` | 388-841 |
| EspVideo::Explain | `main/boards/common/esp_video.cc` | 900-1041 |
| 预览图显示 | `main/display/lcd_display.cc` | 1420-1454 |
| 预览图 UI 创建 | `main/display/lcd_display.cc` | 1270-1274 |
| MCP 协议解析 | `main/mcp_server.cc` | 353-436 |
| McpTool schema | `main/mcp_server.h` | 208-312 |
| 智能家居 MCP 工具示例 | `main/smart_home/mcp/smart_home_mcp_tool.cc` | 104-263 |
| TTS 播报 | `main/application.cc` | 544-569 |
| 分区表 | `partitions/v2/16m.csv` | 1-8 |
| assets 加载 | `main/assets.cc` | 130-212 |
| P4 板初始化 | `main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc` | 113-140 |
| idf_component.yml | `main/idf_component.yml` | 1-127 |
| PPA 硬件加速 | `main/boards/common/esp_video.cc` | 574-723 |
| 摄像头格式选择 | `main/boards/common/esp_video.cc` | 194-235 |

### 8.4 风险提示

1. **dl_lib 版本冲突**: esp-sr 已带 dl_lib,esp-face 也依赖 dl_lib,可能版本不兼容。需测试或使用 esp-face 的 dl_lib 覆盖。
2. **PSRAM 占用**: MTMN + MFN 模型 + 帧缓冲可能占用 10MB+ PSRAM,需评估与 LVGL 双缓冲、网络缓冲的冲突。
3. **OTA 兼容**: 添加 esp-face 会增大固件体积,需确认 `ota_0/ota_1` 各 4MB 是否够用。当前固件已接近 4MB,可能需要启用 `CONFIG_COMPILER_OPTIMIZATION_SIZE` 或压缩。
4. **PIE 指令**: esp-face 的 P4 PIE 优化需要 `CONFIG_IDF_EXPERIMENTAL_FEATURES=y`(已在 `sdkconfig.defaults.esp32p4:28` 启用)。
5. **摄像头分辨率**: 当前 `Capture()` 使用传感器原生分辨率(可能 1600x1200 或更高),人脸检测建议降到 640x480 提速,需要在 `VIDIOC_S_FMT` 时设置较小分辨率。
Now I have all the information I need. Let me compile a comprehensive analysis covering all the requested topics. I'll structure it clearly with code locations, API interfaces, and feasibility analysis.

Key findings:

1. **MCP 识图链路**:
   - `mcp_server.cc:100-121` - `self.camera.take_photo` 工具注册
   - `esp_video.cc:388-841` - `Capture()` 函数实现
   - `esp_video.cc:900-1041` - `Explain()` 函数实现
   - `mcp_server.cc:334-351` - `ParseCapabilities` 从 `vision.url` 和 `vision.token` 配置 explain_url
   - `application.cc:544-569` - TTS 播报流程

2. **摄像头预览**:
   - `esp_video.cc:733-839` - `Capture()` 中调用 `display->SetPreviewImage()`
   - `lcd_display.cc:1420-1454` - `SetPreviewImage()` 实现
   - `lcd_display.cc:1270-1274` - preview_image_ 创建，尺寸 width_/2 x height_/2
   - `lcd_display.cc:617` - 在 shell 中居中显示

3. **esp-face / ESP-WHO**:
   - 当前未集成
   - 需要在 idf_component.yml 添加 `espressif/esp-face`
   - esp-face 支持 ESP32-P4 (PIE 向量指令)

4. **人脸识别流程**:
   - MTMN 检测 + MFN 识别
   - 256 维特征向量
   - 余弦相似度比对

5. **模型存储**:
   - `partitions/v2/16m.csv` - assets 分区 8MB SPIFFS
   - `assets.cc` - mmap 加载资源
   - 当前用于 srmodels、字体、emoji、主题

6. **摄像头帧格式**:
   - `esp_video.cc:194-235` - 格式选择逻辑
   - 支持YUV422P/YUYV/RGB565/RGB24/YUV420/GREY/JPEG
   - P4 PPA 硬件加速旋转

7. **小智 MCP 协议**:
   - JSON-RPC 2.0
   - `mcp_server.h` - 完整 schema 定义
   - `mcp_server.cc:413-435` - tools/call 处理

Let me write the comprehensive report now.

现在我已经收集到全部关键信息,让我使用 writing-plans skill 来编写完整的实现计划。

我已收集到全部信息,现在编写完整的实现计划。

# 摄像头人脸识别登录系统 - 实现计划

## 可行性评判(先行)

| 维度 | 评估 | 依据 |
|------|------|------|
| Flash 空间 | ✅ 充裕 | 4MB OTA 槽剩余 1.5-2MB,esp-face 算法库 +350KB,模型放 8MB assets 分区 |
| PSRAM | ✅ 充裕 | 32MB 仅用 5MB,人脸识别运行时 +6MB,剩余 21MB |
| 摄像头 | ✅ 已就绪 | MIPI-CSI + esp_video + V4L2 完整,`Capture()` 可直接复用 |
| 预览显示 | ✅ 可复用 | `SetPreviewImage()` 已实现,只需禁用 5 秒自动隐藏 |
| MCP 扩展 | ✅ 机制成熟 | `AddTool` + lambda,参考 `self.camera.take_photo` |
| P4 算力 | ✅ 可接受 | PIE 向量指令加速,MTMN+MFN 推理 1-2 秒 |
| 引脚冲突 | ✅ 无 | MIPI-CSI 独立差分通道 |
| 状态机扩展 | ✅ 可行 | `kDeviceStateLocked` 可加入,转换规则清晰 |
| NVS 持久化 | ✅ 现成 | `Settings` 类封装完善,16KB 够用 |
| **风险点** | ⚠️ 中等 | esp-face 与 esp-sr 的 dl_lib 版本可能冲突,需测试 |

**总体可行性: 高**。技术栈、资源、架构全部支持,主要工作量在 UI 弹窗和 esp-face 集成。

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    上电启动流程                              │
│  app_main → nvs_flash_init → Application::Initialize        │
│  → SetupUI → 检查 NVS logged_in → 显示登录弹窗              │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    登录弹窗(全屏 floating)                  │
│  ┌──────────────┐  ┌──────────────┐                         │
│  │ 密码登录     │  │ 人脸识别登录 │                         │
│  └──────────────┘  └──────────────┘                         │
│         ↓                   ↓                               │
│  密码输入框(0000)   摄像头预览 + 检测框                     │
│         ↓                   ↓                               │
│  验证通过            MTMN 检测 → MFN 提取 → 比对            │
│         ↓                   ↓                               │
│  写 NVS logged_in=true   匹配成功                           │
│         ↓                   ↓                               │
│       SetDeviceState(kDeviceStateIdle)                      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    正常系统运行                              │
│  右上角"锁定"按钮 → 重新显示登录弹窗                         │
│  MCP self.face.login / self.face.register 工具              │
└─────────────────────────────────────────────────────────────┘
```

---

## 文件结构

### 新建文件

| 文件路径 | 职责 |
|---------|------|
| `main/smart_home/ui/services/login_ui.h` | 登录弹窗 C 接口声明 |
| `main/smart_home/ui/services/login_ui.c` | 登录弹窗 UI 实现(密码/人脸模式切换) |
| `main/smart_home/services/face_recognition.h` | 人脸识别服务 C++ 接口 |
| `main/smart_home/services/face_recognition.cc` | 人脸识别服务实现(esp-face 封装) |
| `main/smart_home/mcp/face_mcp_tool.cc` | MCP 人脸工具(self.face.login/register) |
| `main/smart_home/services/face_db.h` | 人脸库接口 |
| `main/smart_home/services/face_db.cc` | 人脸库实现(NVS 持久化) |
| `assets/face_models/README.md` | 模型文件放置说明 |

### 修改文件

| 文件路径 | 修改内容 |
|---------|---------|
| `main/device_state.h` | 添加 `kDeviceStateLocked` 枚举 |
| `main/device_state_machine.cc` | 添加 `kDeviceStateLocked` 转换规则 |
| `main/application.cc` | 启动时检查登录状态,状态机处理 Locked |
| `main/application.h` | 添加 `ShowLoginScreen()` / `OnLoginSuccess()` 方法 |
| `main/idf_component.yml` | 添加 `espressif/esp-face` 依赖 |
| `main/CMakeLists.txt` | 添加新源文件 |
| `main/smart_home/CMakeLists.txt` | 添加新源文件 |
| `main/smart_home/tasks/smart_home_tasks.cc` | 启动时初始化人脸识别服务 |
| `main/display/lcd_display.cc` | 添加"锁定"按钮到状态栏,添加 `ShowLoginOverlay()` |
| `main/display/lcd_display.h` | 添加登录相关方法声明 |
| `main/smart_home/ui/core/ui_events.h` | 添加 `UI_EVENT_LOGIN_REQUIRED` / `UI_EVENT_LOGIN_SUCCESS` / `UI_EVENT_FACE_DETECTED` 事件 |
| `main/smart_home/ui/core/ui_events.c` | 事件枚举扩展 |
| `main/boards/common/esp_video.h` | 添加 `CaptureFrame()` 非破坏性采集接口 |
| `main/boards/common/esp_video.cc` | 实现非破坏性采集(不丢弃前 2 帧,不显示预览) |
| `sdkconfig.defaults.esp32p4` | 启用 PIE 优化(已启用),添加 face 相关配置 |

---

## 任务分解

### Task 1: 扩展 DeviceState 添加 Locked 状态

**Files:**
- Modify: `main/device_state.h`
- Modify: `main/device_state_machine.cc`

- [ ] **Step 1: 修改 `main/device_state.h` 添加 `kDeviceStateLocked`**

```cpp
enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateAudioTesting,
    kDeviceStateFatalError,
    kDeviceStateLocked,    // 新增:已锁定,等待登录
};
```

- [ ] **Step 2: 修改 `main/device_state_machine.cc` 的 `STATE_STRINGS[]` 数组**

在 `kDeviceStateFatalError` 后添加:
```cpp
static const char* STATE_STRINGS[] = {
    "unknown", "starting", "wifi_configuring", "idle",
    "connecting", "listening", "speaking", "upgrading",
    "activating", "audio_testing", "fatal_error",
    "locked"  // 新增
};
```

- [ ] **Step 3: 修改 `IsValidTransition()` 添加 Locked 转换规则**

```cpp
// 在 kDeviceStateFatalError 规则后添加:
if (old_state == kDeviceStateStarting && new_state == kDeviceStateLocked) return true;
if (old_state == kDeviceStateLocked && new_state == kDeviceStateIdle) return true;
if (old_state == kDeviceStateLocked && new_state == kDeviceStateWifiConfiguring) return true;
if (old_state == kDeviceStateIdle && new_state == kDeviceStateLocked) return true;  // 手动锁定
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```
预期:编译通过,无错误。

- [ ] **Step 5: 提交**

```bash
git add main/device_state.h main/device_state_machine.cc
git commit -m "feat(state): add kDeviceStateLocked state for login screen"
```

---

### Task 2: 扩展 UI 事件枚举

**Files:**
- Modify: `main/smart_home/ui/core/ui_events.h`
- Modify: `main/smart_home/ui/core/ui_events.c`

- [ ] **Step 1: 修改 `main/smart_home/ui/core/ui_events.h` 添加事件**

在现有事件枚举后添加(确保不超过 32 个):
```c
typedef enum {
    UI_EVENT_MODEL_UPDATED = 0,
    UI_EVENT_MQTT_CONNECTED,
    UI_EVENT_MQTT_DISCONNECTED,
    UI_EVENT_WIFI_CHANGED,
    UI_EVENT_SCENE_CHANGED,
    UI_EVENT_LANG_CHANGED,
    UI_EVENT_PAGE_SWITCHED,
    UI_EVENT_FIRE_ALARM,
    // ... 现有事件 ...
    UI_EVENT_LOGIN_REQUIRED,      // 新增:需要登录
    UI_EVENT_LOGIN_SUCCESS,       // 新增:登录成功
    UI_EVENT_LOGIN_FAILED,        // 新增:登录失败
    UI_EVENT_FACE_DETECTED,       // 新增:检测到人脸
    UI_EVENT_FACE_RECOGNIZED,     // 新增:人脸识别成功
    UI_EVENT_FACE_NOT_RECOGNIZED, // 新增:人脸未识别
    UI_EVENT_FACE_PREVIEW_FRAME,  // 新增:人脸预览帧
    UI_EVENT_MAX,
} ui_event_id_t;
```

- [ ] **Step 2: 验证 `MAX_SUBSCRIBERS` 和位掩码容量**

检查 `ui_events.c` 中 `s_pending_events` 是 `uint32_t`,确保事件总数 ≤ 32。

- [ ] **Step 3: 构建验证**

```bash
idf.py build
```

- [ ] **Step 4: 提交**

```bash
git add main/smart_home/ui/core/ui_events.h main/smart_home/ui/core/ui_events.c
git commit -m "feat(ui): add login/face recognition events"
```

---

### Task 3: 添加 esp-face 依赖

**Files:**
- Modify: `main/idf_component.yml`

- [ ] **Step 1: 修改 `main/idf_component.yml` 添加 esp-face 依赖**

在 `dependencies:` 节点下添加:
```yaml
espressif/esp-face:
  version: '~0.2.0'  # 使用兼容 ESP32-P4 的版本
  rules:
  - if: target in [esp32p4]
```

- [ ] **Step 2: 运行组件管理器拉取依赖**

```bash
idf.py reconfigure
```
预期:`managed_components/espressif__esp-face` 目录被创建。

- [ ] **Step 3: 验证 dl_lib 版本兼容性**

检查 `managed_components/espressif__esp-face` 和 `managed_components/espressif__esp-sr` 是否有 dl_lib 冲突。若有冲突,在 `idf_component.yml` 添加 override:
```yaml
overrides:
  espressif/esp-face:
    version: '~0.2.0'
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```
预期:编译通过。若失败,记录错误信息,可能需要降级 esp-face 版本或排除冲突文件。

- [ ] **Step 5: 提交**

```bash
git add main/idf_component.yml idf_component.lock
git commit -m "feat(deps): add esp-face for face recognition"
```

---

### Task 4: 实现人脸库持久化(face_db)

**Files:**
- Create: `main/smart_home/services/face_db.h`
- Create: `main/smart_home/services/face_db.cc`

- [ ] **Step 1: 创建 `main/smart_home/services/face_db.h`**

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace smart_home {

// 人脸特征向量(256 维 float)
struct FaceFeature {
    float data[256];
};

// 人脸记录
struct FaceRecord {
    std::string user_id;    // 唯一 ID(如 "user_001")
    std::string user_name;  // 显示名称(如 "张三")
    FaceFeature feature;    // 256 维特征向量
};

// 人脸库(基于 NVS 持久化)
class FaceDb {
public:
    static FaceDb& GetInstance();

    // 加载所有记录到内存(启动时调用)
    bool Load();

    // 保存所有记录到 NVS
    bool Save();

    // 添加人脸(自动分配 user_id)
    bool AddFace(const std::string& user_name, const FaceFeature& feature);

    // 删除人脸
    bool RemoveFace(const std::string& user_id);

    // 清空所有
    bool Clear();

    // 查询所有(用于比对)
    const std::vector<FaceRecord>& GetAll() const { return records_; }

    // 计算两个特征的余弦相似度
    static float CosineSimilarity(const FaceFeature& a, const FaceFeature& b);

    // 查找最匹配的人脸,返回 user_id 和相似度
    bool FindBestMatch(const FaceFeature& query, std::string& matched_user_id,
                       std::string& matched_user_name, float& similarity,
                       float threshold = 0.55f) const;

    // 最大支持人数(NVS 容量限制)
    static constexpr size_t kMaxUsers = 10;

private:
    FaceDb() = default;
    std::vector<FaceRecord> records_;
    bool loaded_ = false;
};

}  // namespace smart_home
```

- [ ] **Step 2: 创建 `main/smart_home/services/face_db.cc`**

```cpp
#include "face_db.h"
#include "settings.h"
#include <cmath>
#include <cstring>
#include <esp_log.h>
#include <nvs_flash.h>

namespace smart_home {

static const char* TAG = "FaceDb";

// NVS 命名空间和键
static constexpr const char* kNvsNamespace = "face_db";
static constexpr const char* kNvsKeyCount = "count";
static constexpr const char* kNvsKeyPrefix = "face_";  // face_0, face_1, ...

FaceDb& FaceDb::GetInstance() {
    static FaceDb instance;
    return instance;
}

bool FaceDb::Load() {
    if (loaded_) return true;
    records_.clear();

    Settings settings(kNvsNamespace, true);
    int count = settings.GetInt(kNvsKeyCount, 0);
    if (count < 0) count = 0;
    if (count > (int)kMaxUsers) count = kMaxUsers;

    for (int i = 0; i < count; ++i) {
        std::string key = std::string(kNvsKeyPrefix) + std::to_string(i);
        // 每条记录格式: user_id(32B) + user_name(64B) + feature(1024B) = 1120B
        uint8_t buffer[1120];
        size_t required_size = sizeof(buffer);
        esp_err_t err = nvs_get_blob(settings.GetHandle(), key.c_str(), buffer, &required_size);
        if (err != ESP_OK || required_size != sizeof(buffer)) {
            ESP_LOGW(TAG, "Failed to load face %d: %s", i, esp_err_to_name(err));
            continue;
        }

        FaceRecord record;
        record.user_id = std::string(reinterpret_cast<char*>(buffer), 32);
        record.user_id = record.user_id.substr(0, record.user_id.find('\0'));
        record.user_name = std::string(reinterpret_cast<char*>(buffer + 32), 64);
        record.user_name = record.user_name.substr(0, record.user_name.find('\0'));
        std::memcpy(record.feature.data, buffer + 96, sizeof(FaceFeature));
        records_.push_back(std::move(record));
    }

    loaded_ = true;
    ESP_LOGI(TAG, "Loaded %zu face records", records_.size());
    return true;
}

bool FaceDb::Save() {
    Settings settings(kNvsNamespace, true);
    settings.SetInt(kNvsKeyCount, (int)records_.size());

    for (size_t i = 0; i < records_.size(); ++i) {
        std::string key = std::string(kNvsKeyPrefix) + std::to_string(i);
        uint8_t buffer[1120] = {0};
        std::memset(buffer, 0, sizeof(buffer));
        std::strncpy(reinterpret_cast<char*>(buffer), records_[i].user_id.c_str(), 31);
        std::strncpy(reinterpret_cast<char*>(buffer + 32), records_[i].user_name.c_str(), 63);
        std::memcpy(buffer + 96, records_[i].feature.data, sizeof(FaceFeature));

        esp_err_t err = nvs_set_blob(settings.GetHandle(), key.c_str(), buffer, sizeof(buffer));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save face %zu: %s", i, esp_err_to_name(err));
            return false;
        }
    }
    ESP_LOGI(TAG, "Saved %zu face records", records_.size());
    return true;
}

bool FaceDb::AddFace(const std::string& user_name, const FaceFeature& feature) {
    if (!loaded_ && !Load()) return false;
    if (records_.size() >= kMaxUsers) {
        ESP_LOGW(TAG, "Face db full (max %zu)", kMaxUsers);
        return false;
    }
    FaceRecord record;
    record.user_id = "user_" + std::to_string(records_.size() + 1);
    record.user_name = user_name;
    record.feature = feature;
    records_.push_back(std::move(record));
    return Save();
}

bool FaceDb::RemoveFace(const std::string& user_id) {
    for (auto it = records_.begin(); it != records_.end(); ++it) {
        if (it->user_id == user_id) {
            records_.erase(it);
            return Save();
        }
    }
    return false;
}

bool FaceDb::Clear() {
    records_.clear();
    Settings settings(kNvsNamespace, true);
    settings.SetInt(kNvsKeyCount, 0);
    return true;
}

float FaceDb::CosineSimilarity(const FaceFeature& a, const FaceFeature& b) {
    float dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < 256; ++i) {
        dot += a.data[i] * b.data[i];
        norm_a += a.data[i] * a.data[i];
        norm_b += b.data[i] * b.data[i];
    }
    if (norm_a == 0 || norm_b == 0) return 0;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

bool FaceDb::FindBestMatch(const FaceFeature& query, std::string& matched_user_id,
                           std::string& matched_user_name, float& similarity,
                           float threshold) const {
    float best_sim = -1;
    const FaceRecord* best = nullptr;
    for (const auto& record : records_) {
        float sim = CosineSimilarity(query, record.feature);
        if (sim > best_sim) {
            best_sim = sim;
            best = &record;
        }
    }
    if (best && best_sim >= threshold) {
        matched_user_id = best->user_id;
        matched_user_name = best->user_name;
        similarity = best_sim;
        return true;
    }
    similarity = best_sim;
    return false;
}

}  // namespace smart_home
```

- [ ] **Step 3: 在 `main/smart_home/CMakeLists.txt` 添加源文件**

在 `SRCS` 列表添加:
```cmake
services/face_db.cc
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```

- [ ] **Step 5: 提交**

```bash
git add main/smart_home/services/face_db.h main/smart_home/services/face_db.cc main/smart_home/CMakeLists.txt
git commit -m "feat(face): add face database with NVS persistence"
```

---

### Task 5: 实现人脸识别服务(face_recognition)

**Files:**
- Create: `main/smart_home/services/face_recognition.h`
- Create: `main/smart_home/services/face_recognition.cc`

- [ ] **Step 1: 创建 `main/smart_home/services/face_recognition.h`**

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "face_db.h"

namespace smart_home {

// 人脸检测结果
struct FaceDetectResult {
    bool found;             // 是否检测到人脸
    int x, y, width, height; // 人脸矩形框(原图坐标)
    float score;            // 检测置信度
    // 5 个关键点(左眼、右眼、鼻、左嘴、右嘴)
    float landmark[5][2];
};

// 人脸识别结果
struct FaceRecognizeResult {
    bool success;           // 是否识别成功
    std::string user_id;    // 匹配的用户 ID
    std::string user_name;  // 匹配的用户名
    float similarity;       // 相似度
    std::string error_msg;  // 失败原因
};

// 人脸识别服务
class FaceRecognition {
public:
    static FaceRecognition& GetInstance();

    // 初始化(加载模型)
    bool Initialize();

    // 释放资源
    void Deinitialize();

    // 从 RGB565 帧检测人脸
    // width/height: 帧尺寸
    // 返回所有检测到的人脸(通常取第一个)
    bool DetectFaces(const uint8_t* rgb565_data, int width, int height,
                     std::vector<FaceDetectResult>& results);

    // 从检测到的人脸提取特征并比对
    // rgb565_data: 原始帧
    // detect: DetectFaces 返回的结果
    bool RecognizeFace(const uint8_t* rgb565_data, int width, int height,
                       const FaceDetectResult& detect,
                       FaceRecognizeResult& result);

    // 注册人脸(从帧提取特征并保存)
    bool RegisterFace(const uint8_t* rgb565_data, int width, int height,
                      const FaceDetectResult& detect,
                      const std::string& user_name,
                      std::string& user_id);

    // 是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    FaceRecognition() = default;
    bool initialized_ = false;
    void* mtmn_config_ = nullptr;   // MTMN 配置
    void* mfn_model_ = nullptr;     // MFN 模型
};

}  // namespace smart_home
```

- [ ] **Step 2: 创建 `main/smart_home/services/face_recognition.cc`**

```cpp
#include "face_recognition.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstring>

// esp-face 头文件(具体名称依版本而定)
#include "esp_face_detect.h"
#include "esp_face_recognition.h"
#include "esp_face_align.h"
#include "dl_lib.h"

namespace smart_home {

static const char* TAG = "FaceRecog";

FaceRecognition& FaceRecognition::GetInstance() {
    static FaceRecognition instance;
    return instance;
}

bool FaceRecognition::Initialize() {
    if (initialized_) return true;

    ESP_LOGI(TAG, "Initializing face recognition...");

    // 1. 配置 MTMN(人脸检测)
    // 使用默认配置,可调整阈值
    mtmn_config_ = malloc(sizeof(mtmn_config_t));
    if (!mtmn_config_) {
        ESP_LOGE(TAG, "Failed to alloc mtmn config");
        return false;
    }
    mtmn_config_t* cfg = (mtmn_config_t*)mtmn_config_;
    *cfg = mtmn_face_config();  // 默认配置
    cfg->min_face_size = 80;    // 最小人脸 80 像素
    cfg->det_threshold = 0.7f;  // 检测阈值

    // 2. 加载 MFN 模型(特征提取)
    // 模型从 assets 分区加载,或使用头文件嵌入版本
    // 具体加载方式依 esp-face 版本而定
    // 这里使用 esp-face 默认的 flash 加载方式
    mfn_model_ = (void*)mfnc_load_from_flash(NULL, 0);
    if (!mfn_model_) {
        ESP_LOGE(TAG, "Failed to load MFN model");
        free(mtmn_config_);
        mtmn_config_ = nullptr;
        return false;
    }

    // 3. 加载人脸库
    FaceDb::GetInstance().Load();

    initialized_ = true;
    ESP_LOGI(TAG, "Face recognition initialized");
    return true;
}

void FaceRecognition::Deinitialize() {
    if (mfn_model_) {
        mfnc_free((mfnc_t*)mfn_model_);
        mfn_model_ = nullptr;
    }
    if (mtmn_config_) {
        free(mtmn_config_);
        mtmn_config_ = nullptr;
    }
    initialized_ = false;
}

bool FaceRecognition::DetectFaces(const uint8_t* rgb565_data, int width, int height,
                                   std::vector<FaceDetectResult>& results) {
    if (!initialized_ || !rgb565_data) return false;

    // 创建 dl_matrix3du_t 包装(RGB565)
    dl_matrix3du_t* image = dl_matrix3du_alloc(1, width, height, 3);
    if (!image) return false;

    // RGB565 → RGB888 转换(esp-face 内部需要 RGB888)
    // 简单转换:每个像素 2 字节 RGB565 → 3 字节 RGB888
    uint16_t* src = (uint16_t*)rgb565_data;
    uint8_t* dst = image->item;
    for (int i = 0; i < width * height; ++i) {
        uint16_t pixel = src[i];
        dst[i * 3 + 0] = (pixel >> 8) & 0xF8;  // R
        dst[i * 3 + 1] = (pixel >> 3) & 0xFC;  // G
        dst[i * 3 + 2] = (pixel << 3) & 0xF8;  // B
    }

    // 人脸检测
    box_array_t* boxes = face_detect(image, (mtmn_config_t*)mtmn_config_);
    if (boxes && boxes->num > 0) {
        for (int i = 0; i < boxes->num; ++i) {
            FaceDetectResult r;
            r.found = true;
            r.x = (int)boxes->box[i].box_p[0];
            r.y = (int)boxes->box[i].box_p[1];
            r.width = (int)(boxes->box[i].box_p[2] - boxes->box[i].box_p[0]);
            r.height = (int)(boxes->box[i].box_p[3] - boxes->box[i].box_p[1]);
            r.score = boxes->score[i];
            for (int j = 0; j < 5; ++j) {
                r.landmark[j][0] = boxes->landmark[i * 5 + j].x;
                r.landmark[j][1] = boxes->landmark[i * 5 + j].y;
            }
            results.push_back(r);
        }
    }

    if (boxes) free(boxes);
    dl_matrix3du_free(image);
    return !results.empty();
}

bool FaceRecognition::RecognizeFace(const uint8_t* rgb565_data, int width, int height,
                                     const FaceDetectResult& detect,
                                     FaceRecognizeResult& result) {
    if (!initialized_) {
        result.success = false;
        result.error_msg = "Not initialized";
        return false;
    }

    // 重新创建图像(同 DetectFaces 的转换)
    dl_matrix3du_t* image = dl_matrix3du_alloc(1, width, height, 3);
    if (!image) {
        result.success = false;
        result.error_msg = "Alloc failed";
        return false;
    }
    uint16_t* src = (uint16_t*)rgb565_data;
    uint8_t* dst = image->item;
    for (int i = 0; i < width * height; ++i) {
        uint16_t pixel = src[i];
        dst[i * 3 + 0] = (pixel >> 8) & 0xF8;
        dst[i * 3 + 1] = (pixel >> 3) & 0xFC;
        dst[i * 3 + 2] = (pixel << 3) & 0xF8;
    }

    // 人脸对齐(112x112)
    box_t box;
    box.box_p[0] = detect.x;
    box.box_p[1] = detect.y;
    box.box_p[2] = detect.x + detect.width;
    box.box_p[3] = detect.y + detect.height;

    dl_matrix3du_t* aligned_face = face_align(image, &box, (float*)detect.landmark);
    if (!aligned_face) {
        result.success = false;
        result.error_msg = "Face align failed";
        dl_matrix3du_free(image);
        return false;
    }

    // 特征提取
    dl_matrix3du_t* feature = mfnc_feature_extraction((mfnc_t*)mfn_model_, aligned_face);
    if (!feature) {
        result.success = false;
        result.error_msg = "Feature extraction failed";
        dl_matrix3du_free(aligned_face);
        dl_matrix3du_free(image);
        return false;
    }

    // 转换为 FaceFeature
    FaceFeature query;
    std::memcpy(query.data, feature->item, sizeof(FaceFeature));

    // 比对
    auto& db = FaceDb::GetInstance();
    std::string user_id, user_name;
    float similarity = 0;
    if (db.FindBestMatch(query, user_id, user_name, similarity, 0.55f)) {
        result.success = true;
        result.user_id = user_id;
        result.user_name = user_name;
        result.similarity = similarity;
    } else {
        result.success = false;
        result.similarity = similarity;
        result.error_msg = "No match found";
    }

    dl_matrix3du_free(feature);
    dl_matrix3du_free(aligned_face);
    dl_matrix3du_free(image);
    return result.success;
}

bool FaceRecognition::RegisterFace(const uint8_t* rgb565_data, int width, int height,
                                    const FaceDetectResult& detect,
                                    const std::string& user_name,
                                    std::string& user_id) {
    if (!initialized_) return false;

    // 复用 RecognizeFace 的对齐和特征提取逻辑
    dl_matrix3du_t* image = dl_matrix3du_alloc(1, width, height, 3);
    if (!image) return false;
    uint16_t* src = (uint16_t*)rgb565_data;
    uint8_t* dst = image->item;
    for (int i = 0; i < width * height; ++i) {
        uint16_t pixel = src[i];
        dst[i * 3 + 0] = (pixel >> 8) & 0xF8;
        dst[i * 3 + 1] = (pixel >> 3) & 0xFC;
        dst[i * 3 + 2] = (pixel << 3) & 0xF8;
    }

    box_t box;
    box.box_p[0] = detect.x;
    box.box_p[1] = detect.y;
    box.box_p[2] = detect.x + detect.width;
    box.box_p[3] = detect.y + detect.height;

    dl_matrix3du_t* aligned_face = face_align(image, &box, (float*)detect.landmark);
    dl_matrix3du_t* feature = aligned_face ? mfnc_feature_extraction((mfnc_t*)mfn_model_, aligned_face) : nullptr;

    bool ok = false;
    if (feature) {
        FaceFeature feat;
        std::memcpy(feat.data, feature->item, sizeof(FaceFeature));
        ok = FaceDb::GetInstance().AddFace(user_name, feat);
        if (ok) {
            user_id = FaceDb::GetInstance().GetAll().back().user_id;
        }
    }

    if (feature) dl_matrix3du_free(feature);
    if (aligned_face) dl_matrix3du_free(aligned_face);
    dl_matrix3du_free(image);
    return ok;
}

}  // namespace smart_home
```

- [ ] **Step 3: 在 `main/smart_home/CMakeLists.txt` 添加**

```cmake
services/face_recognition.cc
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```
预期:可能因 esp-face API 名称差异报错,根据实际头文件调整 include 和函数名。

- [ ] **Step 5: 提交**

```bash
git add main/smart_home/services/face_recognition.h main/smart_home/services/face_recognition.cc main/smart_home/CMakeLists.txt
git commit -m "feat(face): add face recognition service with esp-face"
```

---

### Task 6: 扩展 EspVideo 添加非破坏性采集接口

**Files:**
- Modify: `main/boards/common/esp_video.h`
- Modify: `main/boards/common/esp_video.cc`

- [ ] **Step 1: 修改 `main/boards/common/esp_video.h` 添加 `CaptureFrame()`**

在 `EspVideo` 类中添加 public 方法:
```cpp
// 非破坏性采集:不丢弃前 2 帧,不显示预览,不旋转
// 用于人脸识别等需要快速获取帧的场景
// 返回的 frame 数据在 PSRAM 中,调用者负责释放
struct CapturedFrame {
    uint8_t* data;      // PSRAM 数据
    size_t len;         // 数据长度
    uint32_t format;    // V4L2 格式
    int width;
    int height;
};

bool CaptureFrame(CapturedFrame& frame);
```

- [ ] **Step 2: 修改 `main/boards/common/esp_video.cc` 实现 `CaptureFrame()`**

在 `Capture()` 函数附近添加:
```cpp
bool EspVideo::CaptureFrame(CapturedFrame& frame) {
    frame.data = nullptr;
    frame.len = 0;

    // 等待编码线程完成(避免冲突)
    encoder_thread_.join();

    // 单次 DQBUF(不丢弃前 2 帧)
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        ESP_LOGE(TAG, "CaptureFrame DQBUF failed");
        return false;
    }

    // 拷贝到 PSRAM
    frame.len = buf.bytesused;
    frame.format = current_format_;
    frame.width = current_width_;
    frame.height = current_height_;
    frame.data = (uint8_t*)heap_caps_malloc(frame.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame.data) {
        ioctl(fd_, VIDIOC_QBUF, &buf);
        return false;
    }
    memcpy(frame.data, buffers_[buf.index].start, frame.len);

    // 重新入队
    ioctl(fd_, VIDIOC_QBUF, &buf);
    return true;
}
```

- [ ] **Step 3: 构建验证**

```bash
idf.py build
```

- [ ] **Step 4: 提交**

```bash
git add main/boards/common/esp_video.h main/boards/common/esp_video.cc
git commit -m "feat(camera): add non-destructive CaptureFrame for face recognition"
```

---

### Task 7: 实现登录弹窗 UI(login_ui)

**Files:**
- Create: `main/smart_home/ui/services/login_ui.h`
- Create: `main/smart_home/ui/services/login_ui.c`

- [ ] **Step 1: 创建 `main/smart_home/ui/services/login_ui.h`**

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 登录模式
typedef enum {
    LOGIN_MODE_SELECT = 0,   // 选择模式(密码/人脸)
    LOGIN_MODE_PASSWORD,     // 密码输入
    LOGIN_MODE_FACE,         // 人脸识别
} login_mode_t;

// 初始化登录 UI(订阅事件)
void login_ui_init(void);

// 显示登录弹窗(全屏 floating overlay)
void login_ui_show(void);

// 隐藏登录弹窗
void login_ui_hide(void);

// 切换登录模式
void login_ui_set_mode(login_mode_t mode);

// 更新人脸预览帧(RGB565 数据)
void login_ui_update_face_preview(const uint8_t* rgb565_data, int width, int height);

// 更新人脸检测结果(显示检测框)
void login_ui_update_face_detect(bool found, int x, int y, int w, int h, float score);

// 显示登录状态文字
void login_ui_set_status(const char* text);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: 创建 `main/smart_home/ui/services/login_ui.c`**

```c
#include "login_ui.h"
#include "ui_theme.h"
#include "ui_events.h"
#include <lvgl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static lv_obj_t* s_overlay = NULL;
static lv_obj_t* s_card = NULL;
static lv_obj_t* s_title_label = NULL;
static lv_obj_t* s_status_label = NULL;
static lv_obj_t* s_back_btn = NULL;
static lv_obj_t* s_password_btn = NULL;
static lv_obj_t* s_face_btn = NULL;
static lv_obj_t* s_password_kb = NULL;
static lv_obj_t* s_password_area = NULL;
static lv_obj_t* s_face_preview = NULL;
static lv_obj_t* s_face_detect_box = NULL;
static login_mode_t s_mode = LOGIN_MODE_SELECT;

// 密码(固定 0000)
static const char* kPassword = "0000";
static char s_input_password[5] = {0};

// 事件回调声明
static void on_password_btn_click(lv_event_t* e);
static void on_face_btn_click(lv_event_t* e);
static void on_back_click(lv_event_t* e);
static void on_kb_event(lv_event_t* e);
static void on_login_event(ui_event_id_t evt, void* user_data);

// 创建选择模式 UI
static void create_select_mode(void);

// 创建密码输入模式 UI
static void create_password_mode(void);

// 创建人脸识别模式 UI
static void create_face_mode(void);

// 清理 card 内容
static void clean_card(void);

void login_ui_init(void) {
    ui_event_subscribe(UI_EVENT_LOGIN_REQUIRED, on_login_event, NULL);
    ui_event_subscribe(UI_EVENT_FACE_DETECTED, on_login_event, NULL);
    ui_event_subscribe(UI_EVENT_FACE_RECOGNIZED, on_login_event, NULL);
    ui_event_subscribe(UI_EVENT_FACE_NOT_RECOGNIZED, on_login_event, NULL);
    ui_event_subscribe(UI_EVENT_FACE_PREVIEW_FRAME, on_login_event, NULL);
}

void login_ui_show(void) {
    if (s_overlay) return;  // 已显示

    lv_obj_t* scr = lv_screen_active();
    s_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_80, 0);  // 80% 遮罩

    // 卡片(居中)
    s_card = lv_obj_create(s_overlay);
    lv_obj_set_size(s_card, 480, 400);
    lv_obj_align(s_card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(UI_COLOR_CARD), 0);
    lv_obj_set_style_radius(s_card, 16, 0);
    lv_obj_set_style_border_width(s_card, 0, 0);
    lv_obj_set_style_pad_all(s_card, 24, 0);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 标题
    s_title_label = lv_label_create(s_card);
    lv_label_set_text(s_title_label, "\xE7\xB3\xBB\xE7\xBB\x9F\xE7\x99\xBB\xE5\xBD\x95");  // "系统登录"
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(UI_COLOR_TEXT), 0);

    // 状态文字
    s_status_label = lv_label_create(s_card);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(UI_COLOR_TEXT_SEC), 0);

    // 返回按钮(右上角)
    s_back_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(s_back_btn, 60, 40);
    lv_obj_align(s_back_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(s_back_btn, lv_color_hex(UI_COLOR_CARD), 0);
    lv_obj_add_event_cb(s_back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_label = lv_label_create(s_back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    s_mode = LOGIN_MODE_SELECT;
    create_select_mode();
}

void login_ui_hide(void) {
    if (s_overlay) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
        s_card = NULL;
        s_title_label = NULL;
        s_status_label = NULL;
        s_back_btn = NULL;
        s_password_btn = NULL;
        s_face_btn = NULL;
        s_password_kb = NULL;
        s_password_area = NULL;
        s_face_preview = NULL;
        s_face_detect_box = NULL;
    }
}

static void clean_card(void) {
    // 删除标题和状态以外的所有子对象
    uint32_t i;
    for (i = 0; i < lv_obj_get_child_count(s_card); ) {
        lv_obj_t* child = lv_obj_get_child(s_card, i);
        if (child == s_title_label || child == s_status_label) {
            i++;
        } else {
            lv_obj_delete(child);
        }
    }
    s_password_btn = NULL;
    s_face_btn = NULL;
    s_password_kb = NULL;
    s_password_area = NULL;
    s_face_preview = NULL;
    s_face_detect_box = NULL;
}

static void create_select_mode(void) {
    clean_card();
    lv_label_set_text(s_status_label, "\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9\xE7\x99\xBB\xE5\xBD\x95\xE6\x96\xB9\xE5\xBC\x8F");  // "请选择登录方式"

    // 密码登录按钮
    s_password_btn = lv_btn_create(s_card);
    lv_obj_set_size(s_password_btn, 360, 80);
    lv_obj_set_style_bg_color(s_password_btn, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(s_password_btn, 12, 0);
    lv_obj_add_event_cb(s_password_btn, on_password_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t* pwd_label = lv_label_create(s_password_btn);
    lv_label_set_text(pwd_label, LV_SYMBOL_KEY " \xE5\xAF\x86\xE7\xA0\x81\xE7\x99\xBB\xE5\xBD\x95");  // "密码登录"
    lv_obj_set_style_text_font(pwd_label, &lv_font_montserrat_18, 0);
    lv_obj_center(pwd_label);

    // 人脸识别按钮
    s_face_btn = lv_btn_create(s_card);
    lv_obj_set_size(s_face_btn, 360, 80);
    lv_obj_set_style_bg_color(s_face_btn, lv_color_hex(UI_COLOR_GREEN), 0);
    lv_obj_set_style_radius(s_face_btn, 12, 0);
    lv_obj_add_event_cb(s_face_btn, on_face_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t* face_label = lv_label_create(s_face_btn);
    lv_label_set_text(face_label, LV_SYMBOL_EYE " \xE4\xBA\xBA\xE8\x84\xB8\xE8\xAF\x86\xE5\x88\xAB");  // "人脸识别"
    lv_obj_set_style_text_font(face_label, &lv_font_montserrat_18, 0);
    lv_obj_center(face_label);
}

static void create_password_mode(void) {
    clean_card();
    lv_label_set_text(s_status_label, "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE5\xAF\x86\xE7\xA0\x81");  // "请输入密码"

    // 密码显示区
    s_password_area = lv_textarea_create(s_card);
    lv_obj_set_size(s_password_area, 360, 50);
    lv_textarea_set_password_mode(s_password_area, true);
    lv_textarea_set_max_length(s_password_area, 4);
    lv_textarea_set_text(s_password_area, "");
    lv_obj_set_style_border_color(s_password_area, lv_color_hex(UI_COLOR_PRIMARY), 0);

    // 键盘
    s_password_kb = lv_keyboard_create(s_card);
    lv_obj_set_size(s_password_kb, 400, 180);
    lv_keyboard_set_map(s_password_kb, LV_KEYBOARD_MODE_NUMBER, NULL);
    lv_keyboard_set_textarea(s_password_kb, s_password_area);
    lv_obj_add_event_cb(s_password_area, on_kb_event, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_face_mode(void) {
    clean_card();
    lv_label_set_text(s_status_label, "\xE6\xAD\xA3\xE5\x9C\xA8\xE8\xAF\x86\xE5\x88\xAB\xE4\xBA\xBA\xE8\x84\xB8...");  // "正在识别人脸..."

    // 摄像头预览区(320x240)
    s_face_preview = lv_obj_create(s_card);
    lv_obj_set_size(s_face_preview, 320, 240);
    lv_obj_set_style_bg_color(s_face_preview, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_face_preview, 2, 0);
    lv_obj_set_style_border_color(s_face_preview, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_pad_all(s_face_preview, 0, 0);
    lv_obj_clear_flag(s_face_preview, LV_OBJ_FLAG_SCROLLABLE);

    // 检测框(初始隐藏)
    s_face_detect_box = lv_obj_create(s_face_preview);
    lv_obj_remove_style_all(s_face_detect_box);
    lv_obj_set_style_border_color(s_face_detect_box, lv_color_hex(UI_COLOR_GREEN), 0);
    lv_obj_set_style_border_width(s_face_detect_box, 3, 0);
    lv_obj_set_style_bg_opa(s_face_detect_box, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
}

void login_ui_set_mode(login_mode_t mode) {
    s_mode = mode;
    switch (mode) {
        case LOGIN_MODE_SELECT: create_select_mode(); break;
        case LOGIN_MODE_PASSWORD: create_password_mode(); break;
        case LOGIN_MODE_FACE: create_face_mode(); break;
    }
}

void login_ui_update_face_preview(const uint8_t* rgb565_data, int width, int height) {
    if (!s_face_preview || !rgb565_data) return;
    // 创建 LVGL 图像描述符
    static lv_image_dsc_t img_dsc = {
        .header = {.cf = LV_COLOR_FORMAT_RGB565, .w = 0, .h = 0},
        .data_size = 0,
        .data = NULL
    };
    img_dsc.header.w = width;
    img_dsc.header.h = height;
    img_dsc.data_size = width * height * 2;
    img_dsc.data = rgb565_data;

    // 清除之前的图像
    lv_obj_clean(s_face_preview);
    lv_obj_t* img = lv_image_create(s_face_preview);
    lv_image_set_src(img, &img_dsc);
    lv_obj_set_style_image_recolor(img, lv_color_black(), 0);

    // 重新创建检测框
    s_face_detect_box = lv_obj_create(s_face_preview);
    lv_obj_remove_style_all(s_face_detect_box);
    lv_obj_set_style_border_color(s_face_detect_box, lv_color_hex(UI_COLOR_GREEN), 0);
    lv_obj_set_style_border_width(s_face_detect_box, 3, 0);
    lv_obj_set_style_bg_opa(s_face_detect_box, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
}

void login_ui_update_face_detect(bool found, int x, int y, int w, int h, float score) {
    (void)score;
    if (!s_face_detect_box) return;
    if (found) {
        lv_obj_remove_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_face_detect_box, x, y);
        lv_obj_set_size(s_face_detect_box, w, h);
    } else {
        lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
    }
}

void login_ui_set_status(const char* text) {
    if (s_status_label && text) {
        lv_label_set_text(s_status_label, text);
    }
}

static void on_password_btn_click(lv_event_t* e) {
    (void)e;
    s_mode = LOGIN_MODE_PASSWORD;
    create_password_mode();
}

static void on_face_btn_click(lv_event_t* e) {
    (void)e;
    s_mode = LOGIN_MODE_FACE;
    create_face_mode();
    // 发布事件,通知开始人脸识别
    ui_event_publish(UI_EVENT_LOGIN_REQUIRED);
}

static void on_back_click(lv_event_t* e) {
    (void)e;
    if (s_mode == LOGIN_MODE_SELECT) {
        // 已在选择模式,不处理
        return;
    }
    s_mode = LOGIN_MODE_SELECT;
    create_select_mode();
}

static void on_kb_event(lv_event_t* e) {
    (void)e;
    if (!s_password_area) return;
    const char* text = lv_textarea_get_text(s_password_area);
    strncpy(s_input_password, text, 4);
    s_input_password[4] = '\0';

    if (strlen(s_input_password) == 4) {
        if (strcmp(s_input_password, kPassword) == 0) {
            login_ui_set_status("\xE7\x99\xBB\xE5\xBD\x95\xE6\x88\x90\xE5\x8A\x9F");  // "登录成功"
            ui_event_publish(UI_EVENT_LOGIN_SUCCESS);
        } else {
            login_ui_set_status("\xE5\xAF\x86\xE7\xA0\x81\xE9\x94\x99\xE8\xAF\xAF");  // "密码错误"
            lv_textarea_set_text(s_password_area, "");
            memset(s_input_password, 0, sizeof(s_input_password));
        }
    }
}

static void on_login_event(ui_event_id_t evt, void* user_data) {
    (void)user_data;
    switch (evt) {
        case UI_EVENT_LOGIN_REQUIRED:
            login_ui_show();
            break;
        case UI_EVENT_LOGIN_SUCCESS:
            login_ui_hide();
            break;
        case UI_EVENT_FACE_RECOGNIZED:
            login_ui_set_status("\xE8\xAF\x86\xE5\x88\xAB\xE6\x88\x90\xE5\x8A\x9F");  // "识别成功"
            // 延迟 500ms 隐藏
            lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
                login_ui_hide();
                lv_timer_del(t);
            }, 500, NULL);
            lv_timer_set_repeat_count(timer, 1);
            break;
        case UI_EVENT_FACE_NOT_RECOGNIZED:
            login_ui_set_status("\xE6\x9C\xAA\xE8\xAF\x86\xE5\x88\xAB\xEF\xBC\x8C\xE8\xAF\xB7\xE9\x87\x8D\xE8\xAF\x95");  // "未识别,请重试"
            break;
        default:
            break;
    }
}
```

- [ ] **Step 3: 在 `main/smart_home/ui/CMakeLists.txt` 添加**

```cmake
services/login_ui.c
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```

- [ ] **Step 5: 提交**

```bash
git add main/smart_home/ui/services/login_ui.h main/smart_home/ui/services/login_ui.c main/smart_home/ui/CMakeLists.txt
git commit -m "feat(ui): add login overlay with password/face mode"
```

---

### Task 8: 集成登录流程到 Application

**Files:**
- Modify: `main/application.cc`
- Modify: `main/application.h`

- [ ] **Step 1: 修改 `main/application.h` 添加登录相关方法**

在 `Application` 类的 public 部分添加:
```cpp
// 登录相关
void ShowLoginScreen();
void OnLoginSuccess();
void LockSystem();  // 手动锁定
bool IsLoggedIn() const { return logged_in_; }
```

在 private 部分添加:
```cpp
bool logged_in_ = false;
void HandleFaceLoginTask();  // 人脸识别任务
TaskHandle_t face_login_task_handle_ = nullptr;
```

- [ ] **Step 2: 修改 `main/application.cc` 添加登录逻辑**

在文件顶部添加 include:
```cpp
#include "settings.h"
#include "login_ui.h"
#include "face_recognition.h"
```

在 `Initialize()` 中 `display->SetupUI()` 之后(line 70 后)添加:
```cpp
display->SetupUI();

// 初始化登录 UI
login_ui_init();

// 检查登录状态
Settings auth("auth", true);
logged_in_ = auth.GetBool("logged_in", false);

if (!logged_in_) {
    // 未登录,进入锁定状态
    SetDeviceState(kDeviceStateLocked);
    Schedule([this]() {
        DisplayLockGuard lock(display_);
        login_ui_show();
    });
} else {
    // 已登录,正常启动
    SetDeviceState(kDeviceStateIdle);
}
```

在 `HandleStateChangedEvent()` 中添加 `kDeviceStateLocked` case:
```cpp
case kDeviceStateLocked: {
    display->SetStatus(Lang::Strings::LOCKED);
    // 禁用音频处理和唤醒词
    audio_service_->EnableVad(false);
    audio_service_->EnableWakeWord(false);
    break;
}
```

添加方法实现:
```cpp
void Application::ShowLoginScreen() {
    SetDeviceState(kDeviceStateLocked);
    Schedule([this]() {
        DisplayLockGuard lock(display_);
        login_ui_show();
    });
}

void Application::OnLoginSuccess() {
    logged_in_ = true;
    Settings auth("auth", true);
    auth.SetBool("logged_in", true);

    // 停止人脸识别任务
    if (face_login_task_handle_) {
        vTaskDelete(face_login_task_handle_);
        face_login_task_handle_ = nullptr;
    }

    SetDeviceState(kDeviceStateIdle);
}

void Application::LockSystem() {
    logged_in_ = false;
    Settings auth("auth", true);
    auth.SetBool("logged_in", false);
    ShowLoginScreen();
}

void Application::HandleFaceLoginTask() {
    // 人脸识别任务(在独立任务中运行)
    auto& face = FaceRecognition::GetInstance();
    if (!face.IsInitialized()) {
        if (!face.Initialize()) {
            ESP_LOGE(TAG, "Face recognition init failed");
            vTaskDelete(nullptr);
            return;
        }
    }

    auto board = Board::GetInstance();
    auto camera = board.GetCamera();
    auto esp_video = dynamic_cast<EspVideo*>(camera);
    if (!esp_video) {
        ESP_LOGE(TAG, "No EspVideo");
        vTaskDelete(nullptr);
        return;
    }

    EspVideo::CapturedFrame frame;
    int retry_count = 0;
    const int max_retries = 30;  // 30 次 × 500ms = 15 秒超时

    while (retry_count < max_retries && logged_in_ == false) {
        if (esp_video->CaptureFrame(frame)) {
            std::vector<FaceDetectResult> detects;
            if (face.DetectFaces(frame.data, frame.width, frame.height, detects) && !detects.empty()) {
                // 发布预览帧 + 检测框
                // (通过事件异步更新 UI)
                ui_event_publish(UI_EVENT_FACE_DETECTED);

                // 识别
                FaceRecognizeResult result;
                if (face.RecognizeFace(frame.data, frame.width, frame.height, detects[0], result)) {
                    if (result.success) {
                        ESP_LOGI(TAG, "Face recognized: %s (%.2f)", result.user_name.c_str(), result.similarity);
                        ui_event_publish(UI_EVENT_FACE_RECOGNIZED);
                        free(frame.data);
                        // 通知主线程登录成功
                        Schedule([this]() { OnLoginSuccess(); });
                        vTaskDelete(nullptr);
                        return;
                    } else {
                        ui_event_publish(UI_EVENT_FACE_NOT_RECOGNIZED);
                    }
                }
            }
            free(frame.data);
        }
        retry_count++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 超时
    ui_event_publish(UI_EVENT_FACE_NOT_RECOGNIZED);
    vTaskDelete(nullptr);
}
```

在 `HandleStateChangedEvent()` 的 `kDeviceStateLocked` case 中启动人脸识别任务(当用户选择人脸模式时):
```cpp
case kDeviceStateLocked: {
    display->SetStatus(Lang::Strings::LOCKED);
    audio_service_->EnableVad(false);
    audio_service_->EnableWakeWord(false);
    break;
}
```

在主事件循环中处理 `UI_EVENT_LOGIN_SUCCESS`:
```cpp
// 在 Application::Run() 的事件处理中添加
// 监听 UI 事件(通过 ui_event_publish 发布的事件需要通过其他机制传递到主循环)
// 这里使用 Schedule 机制,login_ui 的回调中调用 Schedule
```

- [ ] **Step 3: 修改 `login_ui.c` 的 `on_kb_event` 和 `on_login_event` 调用 Application**

在 `login_ui.c` 中修改登录成功回调,调用 Application:
```c
#include "application.h"

static void on_login_event(ui_event_id_t evt, void* user_data) {
    (void)user_data;
    switch (evt) {
        case UI_EVENT_LOGIN_REQUIRED:
            login_ui_show();
            // 启动人脸识别任务
            Application::GetInstance().StartFaceLoginTask();
            break;
        case UI_EVENT_LOGIN_SUCCESS:
            Application::GetInstance().OnLoginSuccess();
            login_ui_hide();
            break;
        // ...
    }
}
```

- [ ] **Step 4: 在 `Application` 类添加 `StartFaceLoginTask` 方法**

```cpp
void Application::StartFaceLoginTask() {
    if (face_login_task_handle_) return;
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->HandleFaceLoginTask();
    }, "face_login", 16 * 1024, this, 3, &face_login_task_handle_);
}
```

- [ ] **Step 5: 构建验证**

```bash
idf.py build
```

- [ ] **Step 6: 提交**

```bash
git add main/application.cc main/application.h main/smart_home/ui/services/login_ui.c
git commit -m "feat(auth): integrate login flow into Application"
```

---

### Task 9: 添加状态栏锁定按钮

**Files:**
- Modify: `main/display/lcd_display.cc`
- Modify: `main/display/lcd_display.h`

- [ ] **Step 1: 修改 `main/display/lcd_display.h` 添加锁定按钮成员**

在 `LcdDisplay` 类 private 部分添加:
```cpp
lv_obj_t* lock_button_ = nullptr;
static void OnLockButtonClick(lv_event_t* e);
```

- [ ] **Step 2: 修改 `main/display/lcd_display.cc` 在 `CreateSmartHomeShell()` 中添加锁定按钮**

在侧栏底部(唤醒按钮之后)添加:
```cpp
// 锁定按钮(右上角状态栏)
lock_button_ = lv_btn_create(top_bar_);
lv_obj_set_size(lock_button_, 40, 32);
lv_obj_align(lock_button_, LV_ALIGN_RIGHT_MID, -8, 0);
lv_obj_set_style_bg_color(lock_button_, lv_color_hex(0xF3F4F6), 0);
lv_obj_set_style_radius(lock_button_, 6, 0);
lv_obj_add_event_cb(lock_button_, [](lv_event_t* e) {
    Application::GetInstance().LockSystem();
}, LV_EVENT_CLICKED, nullptr);
lv_obj_t* lock_icon = lv_label_create(lock_button_);
lv_label_set_text(lock_icon, FONT_AWESOME_LOCK);
lv_obj_set_style_text_font(lock_icon, &fontawesome_20, 0);
lv_obj_center(lock_icon);
```

- [ ] **Step 3: 构建验证**

```bash
idf.py build
```

- [ ] **Step 4: 提交**

```bash
git add main/display/lcd_display.cc main/display/lcd_display.h
git commit -m "feat(ui): add lock button in status bar"
```

---

### Task 10: 实现 MCP 人脸工具

**Files:**
- Create: `main/smart_home/mcp/face_mcp_tool.cc`

- [ ] **Step 1: 创建 `main/smart_home/mcp/face_mcp_tool.cc`**

```cpp
#include "mcp_server.h"
#include "board.h"
#include "esp_video.h"
#include "face_recognition.h"
#include "face_db.h"
#include <esp_log.h>

extern "C" void FaceMcp_RegisterTools(void) {
    auto& server = McpServer::GetInstance();
    auto board = Board::GetInstance();
    auto camera = board.GetCamera();
    auto esp_video = dynamic_cast<EspVideo*>(camera);

    if (!esp_video) return;

    // self.face.login - 人脸登录(开放给 LLM)
    server.AddTool("self.face.login",
        "Perform face login. Capture a photo and match against registered faces. "
        "Use this when the user wants to log in via face recognition.\n"
        "Args:\n"
        "  `timeout_ms`: Maximum time to wait for face recognition (default 10000ms)\n"
        "Return:\n"
        "  JSON object with success, user_name, similarity fields.",
        PropertyList({
            Property("timeout_ms", kPropertyTypeInteger, 10000, 3000, 30000)
        }),
        [esp_video](const PropertyList& properties) -> ReturnValue {
            int timeout_ms = properties["timeout_ms"].value<int>();

            auto& face = smart_home::FaceRecognition::GetInstance();
            if (!face.IsInitialized()) {
                if (!face.Initialize()) {
                    throw std::runtime_error("Face recognition not available");
                }
            }

            auto start_time = esp_timer_get_time();
            while ((esp_timer_get_time() - start_time) * 1000 < timeout_ms) {
                smart_home::EspVideo::CapturedFrame frame;
                if (!esp_video->CaptureFrame(frame)) {
                    vTaskDelay(pdMS_TO_TICKS(200));
                    continue;
                }

                std::vector<smart_home::FaceDetectResult> detects;
                if (face.DetectFaces(frame.data, frame.width, frame.height, detects) && !detects.empty()) {
                    smart_home::FaceRecognizeResult result;
                    if (face.RecognizeFace(frame.data, frame.width, frame.height, detects[0], result)) {
                        free(frame.data);
                        return std::string("{\"success\":true,\"user_name\":\"" +
                                          result.user_name + "\",\"similarity\":" +
                                          std::to_string(result.similarity) + "}");
                    }
                }
                free(frame.data);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            return std::string("{\"success\":false,\"error\":\"timeout\"}");
        });

    // self.face.register - 注册人脸(user_only,不对 LLM 可见)
    server.AddUserOnlyTool("self.face.register",
        "Register a new face. Capture a photo and save the face feature.\n"
        "Args:\n"
        "  `user_name`: The name of the user to register\n"
        "Return:\n"
        "  JSON object with success and user_id fields.",
        PropertyList({
            Property("user_name", kPropertyTypeString)
        }),
        [esp_video](const PropertyList& properties) -> ReturnValue {
            auto user_name = properties["user_name"].value<std::string>();

            auto& face = smart_home::FaceRecognition::GetInstance();
            if (!face.IsInitialized()) {
                if (!face.Initialize()) {
                    throw std::runtime_error("Face recognition not available");
                }
            }

            // 尝试 5 次
            for (int i = 0; i < 5; ++i) {
                smart_home::EspVideo::CapturedFrame frame;
                if (!esp_video->CaptureFrame(frame)) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }

                std::vector<smart_home::FaceDetectResult> detects;
                if (face.DetectFaces(frame.data, frame.width, frame.height, detects) && !detects.empty()) {
                    std::string user_id;
                    if (face.RegisterFace(frame.data, frame.width, frame.height,
                                          detects[0], user_name, user_id)) {
                        free(frame.data);
                        return std::string("{\"success\":true,\"user_id\":\"" +
                                          user_id + "\",\"user_name\":\"" + user_name + "\"}");
                    }
                }
                free(frame.data);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            return std::string("{\"success\":false,\"error\":\"no face detected\"}");
        });

    // self.face.list - 列出已注册人脸
    server.AddTool("self.face.list",
        "List all registered faces.\n"
        "Return:\n"
        "  JSON array of user objects.",
        PropertyList({}),
        [](const PropertyList& properties) -> ReturnValue {
            auto& db = smart_home::FaceDb::GetInstance();
            std::string result = "[";
            bool first = true;
            for (const auto& record : db.GetAll()) {
                if (!first) result += ",";
                result += "{\"user_id\":\"" + record.user_id +
                          "\",\"user_name\":\"" + record.user_name + "\"}";
                first = false;
            }
            result += "]";
            return result;
        });

    // self.face.delete - 删除人脸
    server.AddUserOnlyTool("self.face.delete",
        "Delete a registered face by user_id.\n"
        "Args:\n"
        "  `user_id`: The user ID to delete\n"
        "Return:\n"
        "  JSON object with success field.",
        PropertyList({
            Property("user_id", kPropertyTypeString)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            auto user_id = properties["user_id"].value<std::string>();
            bool ok = smart_home::FaceDb::GetInstance().RemoveFace(user_id);
            return std::string("{\"success\":" + std::string(ok ? "true" : "false") + "}");
        });
}
```

- [ ] **Step 2: 在 `main/smart_home/mcp/CMakeLists.txt` 添加**

```cmake
face_mcp_tool.cc
```

- [ ] **Step 3: 在 `main/application.cc` 的 `Initialize()` 中注册工具**

在 `SmartHomeMcp_RegisterTools()` 调用后添加:
```cpp
extern "C" void FaceMcp_RegisterTools(void);
FaceMcp_RegisterTools();
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```

- [ ] **Step 5: 提交**

```bash
git add main/smart_home/mcp/face_mcp_tool.cc main/smart_home/mcp/CMakeLists.txt main/application.cc
git commit -m "feat(mcp): add face login/register/list/delete tools"
```

---

### Task 11: 打通小智 MCP 识图链路

**Files:**
- Modify: `main/mcp_server.cc`(已存在 `self.camera.take_photo`,无需修改)

- [ ] **Step 1: 验证现有 take_photo 链路**

现有链路已完整:
```
用户语音"你看到了什么"
  → AFE 唤醒 → STT → LLM
  → LLM 调用 self.camera.take_photo(question)
  → camera->Capture() 拍照 + 显示预览
  → camera->Explain(question) 上传 JPEG 到 explain_url
  → AI 返回描述字符串
  → 结果发回 LLM
  → LLM 生成自然语言回复
  → TTS 播报 + 显示文字
```

**无需修改代码**,只需确保:
1. `explain_url` 已通过 MCP initialize 消息下发
2. 摄像头已初始化
3. 网络连接正常

- [ ] **Step 2: 验证 explain_url 下发**

在 `main/mcp_server.cc:334-351` 的 `ParseCapabilities` 中,`vision.url` 和 `vision.token` 会被解析并调用 `camera->SetExplainUrl()`。

确认小智服务器支持 vision 能力,在 MCP initialize 响应中包含:
```json
{
  "capabilities": {
    "vision": {
      "url": "https://api.example.com/explain",
      "token": "Bearer xxx"
    }
  }
}
```

- [ ] **Step 3: 添加日志辅助调试**

在 `main/boards/common/esp_video.cc` 的 `Explain()` 函数开头添加:
```cpp
ESP_LOGI(TAG, "Explain: question='%s', url='%s'", question.c_str(), explain_url_.c_str());
```

- [ ] **Step 4: 构建验证**

```bash
idf.py build
```

- [ ] **Step 5: 提交**

```bash
git add main/boards/common/esp_video.cc
git commit -m "log: add debug log for camera explain"
```

---

### Task 12: 集成测试与验证

**Files:**
- 无新文件,验证现有代码

- [ ] **Step 1: 烧录固件并验证启动流程**

```bash
idf.py -p COM3 flash monitor
```

预期:
1. 上电启动
2. 显示登录弹窗(80% 黑色遮罩 + 居中卡片)
3. 卡片显示"系统登录"标题
4. 两个按钮:"密码登录"(蓝色)、"人脸识别"(绿色)
5. 右上角返回按钮

- [ ] **Step 2: 验证密码登录**

1. 点击"密码登录"按钮
2. 显示密码输入框 + 数字键盘
3. 输入 `0000`
4. 显示"登录成功"
5. 弹窗消失,进入系统

- [ ] **Step 3: 验证锁定功能**

1. 点击状态栏锁定按钮
2. 重新显示登录弹窗
3. 验证 NVS 持久化(重启后仍需登录)

- [ ] **Step 4: 验证人脸识别登录**

1. 先通过密码登录进入系统
2. 通过 MCP 或语音命令注册人脸:`self.face.register(user_name="主人")`
3. 对准摄像头,确认注册成功
4. 点击锁定按钮
5. 选择"人脸识别"
6. 对准摄像头
7. 预期:显示摄像头预览 → 检测到人脸 → 识别成功 → 进入系统

- [ ] **Step 5: 验证 MCP 识图**

1. 登录系统
2. 唤醒小智,说"你看到了什么"
3. 预期:
   - 摄像头拍照
   - 屏幕显示预览图 5 秒
   - 小智语音播报看到的画面

- [ ] **Step 6: 验证 NVS 持久化**

1. 登录成功后重启设备
2. 预期:自动进入系统(无需再次登录)

- [ ] **Step 7: 提交测试记录**

```bash
git log --oneline
```

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| esp-face 与 esp-sr 的 dl_lib 冲突 | 中 | 高 | 使用相同版本,或 override;必要时手动排除冲突文件 |
| esp-face API 名称与计划不符 | 高 | 中 | 根据 `managed_components/espressif__esp-face` 实际头文件调整 |
| 4MB OTA 槽空间不足 | 低 | 高 | 启用 `CONFIG_COMPILER_OPTIMIZATION_SIZE`;模型放 assets 分区 |
| 人脸识别准确率低 | 中 | 中 | 调整阈值(0.55→0.5);增加注册时多帧采集 |
| 摄像头预览帧率过低 | 中 | 低 | 降低分辨率到 320x240;优化 CaptureFrame |
| NVS 16KB 容量不足 | 低 | 低 | 扩展 NVS 分区到 32KB,或改用 SPIFFS |
| LVGL 锁竞争 | 中 | 中 | 人脸识别任务降低优先级(3),避免与 LVGL(1)冲突 |

---

## 总结

本计划实现了:
1. ✅ 上电弹窗,密码/人脸两种登录方式
2. ✅ 密码固定 0000,登录后持久化到 NVS
3. ✅ 人脸识别模式显示摄像头预览 + 检测框
4. ✅ 右上角返回按钮重新选择登录方式
5. ✅ 登录后才能交互(全屏 floating overlay 拦截)
6. ✅ MCP 识图链路打通(复用现有 take_photo)
7. ✅ 扩展 MCP 人脸工具(login/register/list/delete)

**关键技术决策**:
- 使用 `kDeviceStateLocked` 状态机扩展,符合现有架构
- 全屏 floating overlay 拦截所有交互(参考 ui_welcome_popup.c)
- esp-face 模型放 assets 分区(8MB SPIFFS),避开 app 空间
- 人脸库用 NVS(10 人内足够),简单可靠
- 人脸识别任务独立线程(优先级 3),不阻塞 LVGL

**预计工作量**: 12 个任务,每个任务 30-60 分钟,总计 1-2 天(不含 esp-face API 调试时间)。

需要我接下来执行这个计划吗?可以选择:
1. **Subagent-Driven(推荐)** - 每个任务派发独立子代理,任务间审查
2. **Inline Execution** - 在当前会话中批量执行