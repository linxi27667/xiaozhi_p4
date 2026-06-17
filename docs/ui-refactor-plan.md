# UI 重构实施计划

> 本文档供 subagent 独立执行,无需重新理解项目。所有必要上下文都在本文档 + `.trae/rules/project_rules.md` 中。

## 0. 前置必读

执行任何任务前,**必须先读** `.trae/rules/project_rules.md` 了解项目架构、网络栈约束、设备模型、MQTT 协议。

## 1. 不可破坏的红线

| 红线 | 说明 |
|------|------|
| 网络栈 | 不动 `Board::GetInstance().GetNetwork()`、`EspNetwork`、`esp_hosted`、`esp_wifi_remote` 任何代码 |
| 小智语音 | 不动 `Application`、`MqttProtocol`、`WebsocketProtocol`、`ota.cc` 的调用链 |
| MQTT 协议 | 不改 `shared/mqtt_iot_protocol.h` 的数据包结构,从机无需重编 |
| 设备模型接口 | 不改 `mqtt_device_model.h` 的公开接口签名(可加新字段/新函数) |
| 事件机制 | 不改 `ui_events.h` 的事件类型枚举(可加新事件) |
| 从机代码 | 不动 `slave/` 下任何文件 |
| 内存安全 | 页面必须 `lv_malloc`/`lv_free` + `LV_EVENT_DELETE` 回调成对 |

## 2. 任务分解(可并行/串行)

### 任务 A:主题注释修正 + i18n 修复(地基,无依赖)

**文件**:
- `main/smart_home/ui/ui_theme.c`
- `main/smart_home/ui/services/ui_i18n.c`

**改动**:

A1. `ui_theme.c`: 找到 "Dark navy" 注释,改成 "Cloud White theme initialized"

A2. `ui_i18n.c`: 逐条核对 `s_zh[]` 和 `s_en[]` 数组,修复以下已知错误:
- "呼吸" 被截断成 "吸" → 修复 UTF-8 转义完整性
- "观影" 被截断成 "亮" → 修复
- "灯带" 被写成 "灯顶灯" → 修复
- "设备" 被写成 "输出" → 修复
- 检查所有场景名: 回家/睡眠/观影/起夜/雨天收衣/离家/火警演示
- 检查所有效果名: 静态/呼吸/彩虹/警示
- 检查所有设备名: 大门/大厅灯/主卧灯/客厅灯/厕所灯/风扇/晾衣杆/阳台灯/左天窗/右天窗

**验证**: 编译通过,无 warning

---

### 任务 B:抽取 ui_kit 公共组件(地基,无依赖)

**新建文件**:
- `main/smart_home/ui/core/ui_kit.h`
- `main/smart_home/ui/core/ui_kit.c`

**更新文件**:
- `main/smart_home/ui/CMakeLists.txt`(加入 ui_kit.c)

**组件规格**:

```c
// 页面标题行:图标 + 主标题 + 副标题
lv_obj_t *ui_kit_page_title(lv_obj_t *parent, const char *icon, 
                             const char *title, const char *subtitle);

// 状态胶囊:圆角小标签,带颜色圆点
lv_obj_t *ui_kit_status_pill(lv_obj_t *parent, const char *text, 
                              lv_color_t dot_color, lv_color_t bg_color);

// 指标卡:图标 + 标签 + 数值 + 单位,valid=false 时显示"暂无数据"
lv_obj_t *ui_kit_metric_card(lv_obj_t *parent, const char *icon,
                              const char *label, const char *value,
                              const char *unit, bool valid);

// 设备卡:图标 + 名称 + 状态文字 + 状态颜色,点击回调
lv_obj_t *ui_kit_device_card(lv_obj_t *parent, const char *icon,
                              const char *name, const char *status_text,
                              lv_color_t status_color, bool connected,
                              lv_event_cb_t on_click, void *user_data);

// 场景卡:图标 + 名称 + 描述,active=true 高亮蓝边框
lv_obj_t *ui_kit_scene_card(lv_obj_t *parent, const char *icon,
                             const char *name, const char *desc,
                             bool active, bool danger,
                             lv_event_cb_t on_click, void *user_data);

// 设置行:图标 + 标签 + 右侧控件(开关/箭头/自定义)
lv_obj_t *ui_kit_setting_row(lv_obj_t *parent, const char *icon,
                              const char *label, lv_obj_t *right_widget);

// 分组标题:小字灰色标题
lv_obj_t *ui_kit_group_title(lv_obj_t *parent, const char *text);

// 快捷芯片:小圆角按钮,active 高亮
lv_obj_t *ui_kit_chip(lv_obj_t *parent, const char *text, bool active,
                       lv_event_cb_t on_click, void *user_data);

// 统一弹窗基类:遮罩 + 卡片 + 标题 + 关闭按钮,body 由调用方填充
typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *card;
    lv_obj_t *title_label;
    lv_obj_t *body;
    lv_obj_t *close_btn;
} ui_kit_popup_t;

ui_kit_popup_t ui_kit_popup_create(lv_obj_t *parent, const char *title,
                                    lv_color_t accent_color);
void ui_kit_popup_close(ui_kit_popup_t *popup);
```

**实现要求**:
- 全部用 flex 布局,不用 `lv_obj_set_pos`
- 卡片用 `UI_COLOR_CARD` 背景 + `UI_COLOR_BORDER` 边框 + 圆角 `UI_CARD_RADIUS`
- 阴影用 `ui_apply_card_shadow()`(已有)
- 按压反馈用 `ui_apply_press_feedback()`(已有)
- "暂无数据"态:灰色短文字 + 灰色图标,不占满卡
- 离线态用灰色 `UI_COLOR_TEXT_SEC`,不用红色;告警才用 `UI_COLOR_RED`

**验证**: 编译通过,暂不接入页面

---

### 任务 C:总览页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_data.c`

**删除**:
- `ui_brand` 的 "9th" 芯片 Logo 和 "AI赋能设计" 标语调用
- 所有 `lv_obj_set_pos` 硬编码坐标
- `s_x_scale` 缩放逻辑

**新布局**(flex column,从上到下):

```
┌─────────────────────────────────────────┐
│ [图标] 智慧家庭    [MQTT胶囊][WiFi胶囊]  │  页面标题行(ui_kit_page_title)
│        控制面板     [时间][室外天气]     │
├─────────────────────────────────────────┤
│ ┌─家庭概览─────────────────────────────┐│
│ │ 当前场景: 回家    家庭安全: 正常      ││  ui_kit_metric_card × 4
│ │ 控制器: 1F✓ 2F✓ 3F✗  天气: 26°C晴   ││
│ └──────────────────────────────────────┘│
├─────────────────────────────────────────┤
│ ┌─一楼─┐ ┌─二楼─┐ ┌─三楼─┐            │  楼层卡(ui_kit_device_card 风格)
│ │2设备 │ │6设备 │ │4设备 │            │  设备数从 device_model 算
│ │2在线 │ │6在线 │ │0在线 │            │  不写死
│ └──────┘ └──────┘ └──────┘            │
├─────────────────────────────────────────┤
│ [全部灯光][回家][离家][安防][雨天收衣]  │  ui_kit_chip 快捷操作条
└─────────────────────────────────────────┘
```

**数据源**:
- 场景: `device_model_get()->current_scene` + `device_model_scene_name()`
- 控制器在线: `device_model_get()->controller_online[0/1/2]`
- 设备数: 遍历 `device_model_at(i)` 按 `floor` 分类计数
- 天气: `device_model_get()->weather_valid` + `outdoor_temp`
- MQTT: `device_model_get()->mqtt_state`
- 时间: `lv_tick_get()` 或系统时间

**快捷操作**:
- 全部灯光 → `mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF)` (或 ON,根据当前状态切换)
- 回家 → `mqtt_send_scene(IOT_SCENE_HOME)`
- 离家 → `mqtt_send_scene(IOT_SCENE_AWAY)`
- 安防 → `mqtt_send_broadcast(IOT_CMD_BROADCAST_ALL_OFF)`
- 雨天收衣 → `mqtt_send_scene(IOT_SCENE_RAIN)`

**验证**: 编译通过,页面能显示,无数据时显示"暂无数据"不崩溃

---

### 任务 D:控制页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_ctrl.c`

**删除**:
- 所有 `lv_obj_set_pos` 硬编码
- 设计稿里不存在的设备(空调/摄像头/扫地/热水器/新风/窗帘/插座)

**新布局**:

楼层 Tab(顶部按钮组): 一楼 | 二楼 | 三楼

**一楼页**:
- 大门(舵机): ui_kit_device_card,状态"开/关",点击切换
- 大厅灯(开关): ui_kit_device_card,状态"开/关",点击切换

**二楼页**:
- 客厅灯: ui_kit_device_card
- 厕所灯: ui_kit_device_card
- 风扇: ui_kit_device_card
- 二楼晾衣杆(舵机): ui_kit_device_card,状态"收/放"

**三楼页**:
- 阳台灯: ui_kit_device_card
- 左天窗(舵机): ui_kit_device_card,状态"开/关"
- 右天窗(舵机): ui_kit_device_card,状态"开/关"
- 三楼晾衣杆(舵机): ui_kit_device_card,状态"收/放"

**主卧 RGB 灯不在这里**,在灯光页。

**交互**:
- 点击设备卡 → `device_model_toggle_device(index)`
- 舵机类: 点击切换 开↔关(对应角度 0↔180 或 0↔135)
- 灯/继电器: 点击切换 ON↔OFF
- 离线设备: 卡片灰色,点击无反应(或提示"设备离线")

**数据源**: `device_model_at(index)` 遍历,按 `floor` 过滤

**验证**: 编译通过,Tab 切换正常,点击设备能发 MQTT 命令

---

### 任务 E:灯光页重做(依赖 A、B,最重)

**文件**: `main/smart_home/ui/pages/page_light.c`

**删除**:
- 所有 `lv_obj_set_pos` 硬编码
- 旧的色块堆叠实现

**新布局**(flex row):

```
┌──────────────┬──────────────────────┐
│              │ 二楼主卧灯带          │  标题
│   色环       │ [在线]               │  状态胶囊(灰色离线/绿色在线)
│  (lv_arc)    │                      │
│   ◉ 当前色   │ 亮度  ━━━━━●━━━ 60%  │  亮度滑条 0-100
│              │                      │
│              │ 效果                  │
│              │ [静态][呼吸][彩虹][警示]│  4个效果按钮
│              │                      │
│              │ 颜色预设              │
│              │ [暖白][日落][自然]    │  预设色块
│              │ [冷白][夜灯][自定义]  │
└──────────────┴──────────────────────┘
```

**色环实现**:
- 用 `lv_arc_create(parent)`,范围 0-360(色相)
- `LV_EVENT_VALUE_CHANGED` 回调取角度 → HSV(h, 100, 100) → RGB
- 中心放一个 `lv_obj_t` 圆形预览块,背景色 = 当前 RGB
- 旋钮(`lv_arc` 的 knob)颜色 = 当前 RGB

**亮度滑条**:
- `lv_slider_create`,范围 0-100
- 回调 → `mqtt_send_rgb_light(2, red, green, blue, brightness, effect)`

**效果按钮**:
- 4 个 `ui_kit_chip`: 静态(STATIC=0) / 呼吸(BREATHE=1) / 彩虹(RAINBOW=2) / 警示(WARNING=3)
- 当前效果高亮
- 点击 → `mqtt_send_rgb_light(2, r, g, b, brightness, new_effect)`

**颜色预设**(点击设 RGB,不改效果):
- 暖白: RGB(255, 180, 100)
- 日落: RGB(255, 120, 60)
- 自然: RGB(255, 230, 200)
- 冷白: RGB(200, 230, 255)
- 夜灯: RGB(255, 80, 30)
- 自定义: 不改色,只让用户用色环选

**数据源**:
- 找 `floor2_master_light` 设备(`device_model_at` 遍历找 `type == RC_DEVICE_RGB_LIGHT`)
- `power_on` / `red` / `green` / `blue` / `brightness` / `effect` / `connected`
- 在线时色环和滑条可操作,离线时灰色禁用

**标题**: "二楼主卧灯带"(不是"三楼",不是"灯顶灯")

**验证**: 编译通过,色环拖动能改色,滑条能调亮度,效果按钮能切换,离线时禁用

---

### 任务 F:场景页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_scene.c`

**删除**:
- 设计稿的"聚会"和"自动模式"(协议不支持)
- 所有 `lv_obj_set_pos` 硬编码

**新布局**(flex,7个场景卡):

```
┌─────────────────────────────────────┐
│ [图标] 场景模式                     │  页面标题
├─────────────────────────────────────┤
│ ┌回家┐ ┌睡眠┐ ┌观影┐ ┌起夜┐       │  4列 第1行
│ └────┘ └────┘ └────┘ └────┘       │  ui_kit_scene_card
├─────────────────────────────────────┤
│ ┌雨天收衣┐ ┌离家┐                  │  2列 第2行
│ └────────┘ └────┘                  │
├─────────────────────────────────────┤
│ ┌火警演示──────────────────────┐   │  独立一行,红色警示样式
│ │ ⚠️ 触发火警联动演示           │   │  danger=true
│ └──────────────────────────────┘   │
└─────────────────────────────────────┘
```

**场景映射**:
| 卡片 | scene_id | 描述 |
|------|----------|------|
| 回家 | IOT_SCENE_HOME | 开灯+暖色氛围 |
| 睡眠 | IOT_SCENE_SLEEP | 关灯+蓝色夜灯 |
| 观影 | IOT_SCENE_MOVIE | 关灯+影院蓝 |
| 起夜 | IOT_SCENE_NIGHT | 开灯+橙色弱光 |
| 雨天收衣 | IOT_SCENE_RAIN | 收晾衣杆+蓝色呼吸 |
| 离家 | IOT_SCENE_AWAY | 全关 |
| 火警演示 | IOT_SCENE_FIRE | 红色警示+全开 |

**交互**:
- 点击 → `mqtt_send_scene(scene_id)`
- 当前场景高亮(`active=true`,蓝边框)
- 火警卡片 `danger=true`,红色边框+红色图标

**数据源**: `device_model_get()->current_scene` 判断高亮

**验证**: 编译通过,点击能发 MQTT,当前场景高亮

---

### 任务 G:环境页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_env.c`

**删除**:
- 设计稿的"光照强度320lux"(无此传感器)
- 所有 `lv_obj_set_pos` 硬编码

**新布局**(flex column):

```
┌─────────────────────────────────────────┐
│ [图标] 环境  [传感器N在线][天气更新时间] │  标题 + 状态胶囊
├─────────────────────────────────────────┤
│ 室内传感器(从机上报)                    │  ui_kit_group_title
│ ┌二楼雨滴┐ ┌二楼雨滴┐ ┌三楼雨滴┐       │
│ │ 0 mV   │ │ 干燥   │ │ 0 mV   │       │  ui_kit_metric_card
│ └────────┘ └────────┘ └────────┘       │
│ ┌三楼雨滴┐ ┌三楼烟雾┐ ┌三楼火警┐       │
│ │ 干燥   │ │ 0 mV   │ │ 正常   │       │
│ └────────┘ └────────┘ └────────┘       │
│ ┌三楼求助┐                             │
│ │ 正常   │                             │
│ └────────┘                             │
├─────────────────────────────────────────┤
│ 室外天气(Open-Meteo)                   │  ui_kit_group_title
│ ┌温度┐ ┌湿度┐ ┌天气┐ ┌降水┐           │
│ │26°C│ │58%│ │晴  │ │0mm│             │  ui_kit_metric_card
│ └────┘ └────┘ └────┘ └────┘           │
│ ┌风速┐ ┌PM2.5┐ ┌AQI┐                  │
│ │3m/s│ │28  │ │优 │                   │
│ └────┘ └─────┘ ────┘                   │
└─────────────────────────────────────────┘
```

**室内数据源**(`device_model_get()`):
- 二楼雨滴值: `floor2_rain_mv` / `floor2_rain_valid`
- 二楼雨滴状态: `floor2_rain_status` / `floor2_rain_valid`
- 三楼雨滴值: `floor3_rain_mv` / `floor3_rain_valid`
- 三楼雨滴状态: `floor3_rain_status` / `floor3_rain_valid`
- 三楼烟雾: `floor3_smoke_mv` / `floor3_smoke_valid`
- 三楼火警: `floor3_fire_status` / `floor3_fire_valid`
- 三楼求助: `floor3_help_status` / `floor3_help_valid`

**室外数据源**(`device_model_get()`):
- 温度: `outdoor_temp` / `weather_valid`
- 湿度: `outdoor_humidity` / `weather_valid`
- 天气: `weather_code` → 映射成文字(晴/多云/雨/雪)
- 降水: `precipitation_mm` / `weather_valid`
- 风速: `wind_speed` / `weather_valid`
- PM2.5: `pm25_outdoor` / `weather_valid`
- AQI: `aqi` / `weather_valid`

**无数据态**: `valid=false` → ui_kit_metric_card 显示"暂无数据"灰色小字

**天气代码映射**(WMO code):
- 0: 晴 / 1-3: 多云 / 45-48: 雾 / 51-67: 雨 / 71-77: 雪 / 80-82: 阵雨 / 95-99: 雷暴

**验证**: 编译通过,无数据时显示"暂无数据",有数据时正常显示

---

### 任务 H:网络页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_net.c`

**新布局**(flex column):

```
┌─────────────────────────────────────────┐
│ [图标] 网络  [WiFi胶囊][MQTT胶囊]       │  标题 + 状态
├─────────────────────────────────────────┤
│ Wi-Fi                                  │  ui_kit_group_title
│ ┌SSID──────┬信号──────┬IP────────┐    │
│ │xxx       │-45 dBm   │192.168.1.x│    │  ui_kit_setting_row
│ └──────────┴──────────┴──────────┘    │
│ [扫描网络]                             │  按钮
├─────────────────────────────────────────┤
│ MQTT                                   │  ui_kit_group_title
│ ┌Broker────┬Client ID──┬状态──────┐   │
│ │8.134...  │xiaozhi_p4 │已连接    │   │  ui_kit_setting_row
│ └──────────┴──────────┴──────────┘   │
│ ┌消息计数──┬心跳超时──┐              │
│ │1234      │60秒      │              │
│ └──────────┴──────────┘              │
├─────────────────────────────────────────┤
│ [连接WiFi] [断开MQTT]                  │  操作按钮
└─────────────────────────────────────────┘
```

**数据源**:
- WiFi SSID: 从真 `WifiManager`(不是 stub)或显示"未连接"
- MQTT: `device_model_get()->mqtt_state` / `mqtt_broker`
- 消息计数: `device_model_get()->mqtt_msg_count`(如有)

**注意**: WiFi 扫描弹窗保留,视觉改成 Cloud White

**验证**: 编译通过,状态正确显示

---

### 任务 I:设置页重做(依赖 A、B)

**文件**: `main/smart_home/ui/pages/page_set.c`

**新布局**(flex column):

```
┌─────────────────────────────────────────┐
│ [图标] 设置                             │  标题
├─────────────────────────────────────────┤
│ 网络                          >        │  ui_kit_setting_row(箭头)
│ 语言          中文                     │  ui_kit_setting_row(切换)
│ 亮度          ━━━●━━━━━ 60%           │  ui_kit_setting_row(滑条)
│ 声音          ━━━━●━━━━ 70%           │  ui_kit_setting_row(滑条)
├─────────────────────────────────────────┤
│ 关于                                   │  ui_kit_group_title
│ 固件版本      v1.0.0                   │  ui_kit_setting_row
│ MQTT Broker   8.134.167.240:1883       │  ui_kit_setting_row
│ 天气位置      上海                     │  ui_kit_setting_row
│ 设备ID        xiaozhi_p4_host          │  ui_kit_setting_row
└─────────────────────────────────────────┘
```

**交互**:
- 网络行点击 → 切到网络页 `UI_Manager_Switch_Page(UI_PAGE_NET)`
- 语言行点击 → 切换中英文 `ui_i18n_set_lang()`
- 亮度滑条 → 调背光(如有 API)或仅显示
- 声音滑条 → 调音量(如有 API)或仅显示

**验证**: 编译通过,语言切换能触发页面重建

---

### 任务 J:聊天页接入(依赖 A、B,高风险)

**新建文件**:
- `main/smart_home/ui/pages/page_chat.h`
- `main/smart_home/ui/pages/page_chat.c`

**更新文件**:
- `main/smart_home/ui/CMakeLists.txt`(加入 page_chat.c)
- `main/smart_home/ui/ui_manager.h`(加 `UI_PAGE_CHAT`)
- `main/smart_home/ui/ui_manager.c`(加 chat 页注册)
- `main/smart_home/ui/core/ui_events.h`(加 `UI_EVENT_CHAT_MESSAGE` / `UI_EVENT_EMOTION`)
- `main/display/lcd_display.cc`(Chat 页改走 page_chat,SetChatMessage/SetEmotion 加 ui_event_publish)

**page_chat.c 布局**(flex column):

```
┌─────────────────────────────────────────┐
│ [机器人头像]  小智中控台  [连接胶囊]     │  标题行
├─────────────────────────────────────────┤
│                                         │
│  [assistant] 你好,我是小智             │  消息气泡(左)
│                                         │
│              我要回家 [user]            │  消息气泡(右)
│                                         │
│  [assistant] 好的,已切换回家模式       │
│                                         │
│  [system] 场景已切换:回家              │  系统消息(居中灰色)
│                                         │
├─────────────────────────────────────────┤
│ 字幕滚动区(最新一句)                   │  底部字幕
├─────────────────────────────────────────┤
│ [回家][观影][睡眠][关灯][查看设备]      │  ui_kit_chip 快捷建议
└─────────────────────────────────────────┘
```

**消息气泡样式**:
- assistant: 左对齐,白底蓝边框,圆角,前面带机器人图标
- user: 右对齐,蓝底白字,圆角
- system: 居中,灰色小字,无气泡

**数据流**:
```
小智 SetChatMessage(role, text)
  └─ lcd_display.cc 加一行: ui_event_publish(UI_EVENT_CHAT_MESSAGE, {role, text})
       └─ page_chat.c on_chat_message 回调 → 添加气泡

小智 SetEmotion(emotion)
  └─ lcd_display.cc 加一行: ui_event_publish(UI_EVENT_EMOTION, {emotion})
       └─ page_chat.c on_emotion 回调 → 更新头像表情
```

**lcd_display.cc 改动**(最小化):
1. `SwitchShellPage(Chat)`: 不再显示内置聊天 UI,改为 `UI_Manager_Switch_Page(UI_PAGE_CHAT)`
2. `SetChatMessage`: 保留原有逻辑 + 加 `ui_event_publish(UI_EVENT_CHAT_MESSAGE, ...)`
3. `SetEmotion`: 保留原有逻辑 + 加 `ui_event_publish(UI_EVENT_EMOTION, ...)`
4. 唤醒按钮保留不动

**快捷建议按钮**:
- 回家 → `mqtt_send_scene(IOT_SCENE_HOME)`
- 观影 → `mqtt_send_scene(IOT_SCENE_MOVIE)`
- 睡眠 → `mqtt_send_scene(IOT_SCENE_SLEEP)`
- 关灯 → `mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF)`
- 查看设备 → `UI_Manager_Switch_Page(UI_PAGE_DATA)`

**⚠️ 高风险点**:
- `lcd_display.cc` 是 C++ 文件,page_chat.c 是 C 文件,需 `extern "C"` 桥接
- `ui_event_publish` 需要扩展支持携带数据(目前只设标志位),或用单独的 ring buffer 传消息
- 不改 `Application`/`MqttProtocol`/`WebsocketProtocol` 任何代码

**验证**: 编译通过,小智对话消息能显示在聊天页,唤醒按钮仍能工作

---

### 任务 K:侧栏补"唤醒"入口(依赖 J)

**文件**: `main/display/lcd_display.cc`

**改动**:
- `CreateSmartHomeShell`: 侧栏从 8 项扩到 9 项
- 新增"唤醒"按钮(蓝色高亮),点击 → `Application::GetInstance().ToggleChatState()`
- 或把现有底部蓝色唤醒按钮归入侧栏统一管理
- `ShellPage` 枚举加 `WakeUp`
- `RefreshShellNav` 支持第 9 项高亮

**验证**: 编译通过,9 个侧栏入口都能进

---

## 3. 执行顺序

```
阶段0(地基,可并行):
  A(主题+i18n) ∥ B(ui_kit)

阶段1(页面重做,依赖A+B,可并行):
  C(总览) ∥ D(控制) ∥ E(灯光) ∥ F(场景) ∥ G(环境) ∥ H(网络) ∥ I(设置)

阶段2(聊天页,依赖A+B,串行):
  J(聊天页接入) → K(侧栏补唤醒)

阶段3(验证):
  全量编译 + UI 截图对照
```

## 4. 验证标准

每个任务完成后必须满足:
1. `idf.py build` 编译通过,无 error,无新增 warning
2. 不引入新的 `lv_obj_set_pos`(全部用 flex)
3. 不引入新的 PNG/图片资源(只用 Font Awesome + LVGL 矢量)
4. 无数据时显示"暂无数据",不崩溃,不显示假数据
5. 离线设备灰色,告警才红色
6. 中英文切换不崩

## 5. 文件修改清单

| 任务 | 文件 | 操作 |
|------|------|------|
| A | ui_theme.c | 改注释 |
| A | ui_i18n.c | 修字符串 |
| B | core/ui_kit.h | 新建 |
| B | core/ui_kit.c | 新建 |
| B | CMakeLists.txt | 加 ui_kit.c |
| C | pages/page_data.c | 重写 |
| D | pages/page_ctrl.c | 重写 |
| E | pages/page_light.c | 重写 |
| F | pages/page_scene.c | 重写 |
| G | pages/page_env.c | 重写 |
| H | pages/page_net.c | 重写 |
| I | pages/page_set.c | 重写 |
| J | pages/page_chat.h | 新建 |
| J | pages/page_chat.c | 新建 |
| J | ui_manager.h | 加 UI_PAGE_CHAT |
| J | ui_manager.c | 加 chat 注册 |
| J | core/ui_events.h | 加 CHAT_MESSAGE/EMOTION 事件 |
| J | display/lcd_display.cc | Chat 页改走 page_chat |
| K | display/lcd_display.cc | 侧栏加唤醒 |

## 6. subagent 分派建议

| subagent | 任务 | 说明 |
|----------|------|------|
| agent-1 | A + B | 地基,先完成 |
| agent-2 | C + D | 总览+控制 |
| agent-3 | E | 灯光页(最重,单独) |
| agent-4 | F + G | 场景+环境 |
| agent-5 | H + I | 网络+设置 |
| agent-6 | J + K | 聊天页(高风险,最后) |

agent-2~5 等 agent-1 完成后并行启动,agent-6 等 agent-1 完成后启动但与 2~5 并行。
