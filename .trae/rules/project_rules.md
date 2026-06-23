# xiaozhi-for-p4 项目规则与架构认知

> 本文档供 AI 助手快速理解项目,避免每次重新分析。基于 2026-06-16 的代码状态。

## 1. 项目定位

基于小智(Xiaozhi AI 语音助手)的 ESP32-P4 智能家居中控。
- **主机**: ESP32-P4(本仓库 `main/`),运行 LVGL UI + 小智语音 + 智能家居 MQTT
- **从机**: ESP32-S3(`slave/` 下三个子项目),通过 MQTT 接收主机命令控制 GPIO/舵机/传感器
- **网络协处理器**: ESP32-C6 通过 SDIO 给 P4 提供 WiFi(esp-hosted)

## 2. 目录结构

```
xiaozhi-for-p4/
├── main/                          # P4 主机固件
│   ├── application.cc             # 应用主入口
│   ├── ota.cc                     # OTA 升级(获取 MQTT/WS 配置)
│   ├── display/lcd_display.cc     # LVGL 显示 + Shell 容器 + 聊天页
│   ├── boards/                    # 板级抽象
│   │   ├── common/wifi_board.cc   # WiFi/网络/EspNetwork 单例
│   │   └── esp-p4-function-ev-board/  # P4 EV 板(1024x600 MIPI-DSI)
│   ├── protocols/                 # 小智语音协议
│   │   ├── mqtt_protocol.cc       # MQTT 信令 + UDP 音频
│   │   └── websocket_protocol.cc  # WebSocket 语音
│   └── smart_home/                # 智能家居模块
│       ├── ui/                    # LVGL UI(C 语言)
│       │   ├── ui_manager.c       # 页面导航/切换
│       │   ├── ui_theme.c         # Cloud White 主题色板
│       │   ├── core/
│       │   │   ├── ui_styles.c    # 公共 UI 组件工厂
│       │   │   ├── ui_events.c    # 事件发布/订阅(异步)
│       │   │   └── smart_home_alarm_ui.c  # 火警弹窗
│       │   ├── model/
│       │   │   └── mqtt_device_model.c  # 设备模型单例
│       │   ├── pages/
│       │   │   ├── page_data.c    # 总览页
│       │   │   ├── page_ctrl.c    # 控制页
│       │   │   ├── page_light.c   # 灯光页
│       │   │   ├── page_scene.c   # 场景页
│       │   │   ├── page_env.c     # 环境页
│       │   │   ├── page_net.c     # 网络页
│       │   │   └── page_set.c     # 设置页
│       │   └── services/
│       │       ├── ui_i18n.c      # 中英文国际化
│       │       ├── ui_icons.h     # Font Awesome 图标映射
│       │       ├── ui_brand.c     # 品牌 Logo(矢量绘制)
│       │       └── ui_welcome_popup.c  # 欢迎回家弹窗
│       ├── services/
│       │   ├── xiaozhi_mqtt.cc    # 智能家居 MQTT 客户端
│       │   ├── weather_service.cc # Open-Meteo 天气服务
│       │   ├── wifi_manager.cc    # ⚠️ STUB!永远返回 true
│       │   └── network_diag.c     # 网络诊断状态机
│       ├── tasks/smart_home_tasks.cc  # MQTT+天气轮询任务
│       └── mcp/                   # MCP 工具集成
├── slave/                         # S3 从机固件(三个独立项目)
│   ├── xiaozhi_slave_Firstfloor/  # 一楼(大门+大厅灯)
│   ├── xiaozhi_slave_Secondfloor/ # 二楼(主卧RGB+客厅灯+厕所灯+风扇+晾衣杆+雨滴)
│   └── xiaozhi_slave_Thirdfloor/  # 三楼(阳台灯+左右天窗+晾衣杆+雨滴+烟雾+火灾+求助)
├── shared/mqtt_iot_protocol.h     # 共享 MQTT 协议定义
└── sdkconfig.defaults.esp32p4     # P4 默认配置(含 esp-hosted SDIO)
```

## 3. 硬件架构

### 3.1 主机 P4
- **SoC**: ESP32-P4(双核,无内置 WiFi)
- **显示**: 1024×600 MIPI-DSI RGB888,单缓冲,buffer=1024×50
- **网络**: 通过 SDIO 4-bit @ 40MHz 连接 ESP32-C6 协处理器
- **SDIO 引脚**(P4 EV 板): CMD=18/19, CLK=12/19, D0-D3=11/14, 10/15, 9/16, 8/17, Reset=54

### 3.2 从机 S3(三个)
- **SoC**: ESP32-S3
- **通信**: WiFi + MQTT(连同一个 broker `8.134.167.240:1883`)
- **一楼**: 1 舵机(大门) + 1 灯(大厅灯)
- **二楼**: 3 灯(主卧RGB/客厅/厕所) + 1 继电器(风扇) + 1 舵机(晾衣杆) + 雨滴传感器
- **三楼**: 1 灯(阳台) + 3 舵机(左天窗/右天窗/晾衣杆) + 烟雾/雨滴/火灾/求助传感器

## 4. 网络栈(不能破坏)

### 4.1 统一网络抽象
所有网络调用经 `Board::GetInstance().GetNetwork()` 返回 `EspNetwork` 单例:
- `CreateHttp(id)` → `esp_http_client` → LWIP → esp_wifi_remote → SDIO → C6
- `CreateMqtt(id)` → `esp-mqtt` → 同上
- `CreateWebSocket(id)` → `esp_websocket_client` → 同上
- `CreateUdp(id)` → LWIP socket → 同上

**全部共享同一个网络栈,没有第二条路径。**

### 4.2 两个独立 MQTT 客户端
| 通道 | id | 用途 | broker |
|------|----|----|--------|
| 0 | `CreateMqtt(0)` | 小智语音信令 | 动态下发(NVS `mqtt.endpoint`) |
| 1 | `CreateMqtt(1)` | 智能家居 IoT | 硬编码 `8.134.167.240:1883` |

### 4.3 WiFi 配网
- SSID/密码存 NVS(经 `SsidManager`)
- 配网方式: AP热点 / BluFi / 声波(三选一,由 Kconfig 选)
- ⚠️ `smart_home/services/wifi_manager.cc` 是 STUB,`is_connected()` 永远返回 true,不能用于网络就绪判断

### 4.4 启动顺序
```
app_main()
└─ Application::Initialize()
   ├─ SmartHomeTasksStart()     # 先启动,UI不依赖网络
   │  └─ smart_home_mqtt_task (Core1, prio5, stack8K)
   │     └─ 每1秒: mqtt_client_poll() + weather_service_poll()
   └─ network_start task
      └─ Board::StartNetwork()  # WiFi 连接
         └─ NetworkEvent::Connected → OTA → 写入 MQTT/WS 配置
```

## 5. 小智语音链路(不能破坏)

### 5.1 协议
- `MqttProtocol`: MQTT 信令 + UDP 加密 OPUS 音频
- `WebsocketProtocol`: WebSocket 全双工
- endpoint 从 NVS 读(`mqtt.endpoint` / `websocket.endpoint`),由 OTA 激活时下发

### 5.2 UI 接口
- `SetEmotion(emotion)`: 设置表情
- `SetChatMessage(role, text)`: 添加对话消息
- 这两个接口在 `lcd_display.cc` 实现,目前只更新内置聊天 UI

### 5.3 唤醒
- 侧栏底部蓝色按钮 → `Application::GetInstance().ToggleChatState()`
- 语音唤醒词由小智引擎处理

## 6. 智能家居 MQTT 协议

### 6.1 Topic 体系
| Topic | 方向 | QoS | 用途 |
|-------|------|-----|------|
| `xiaozhi/iot/cmd/broadcast` | 主机→所有从机 | 0 | 广播命令 |
| `xiaozhi/iot/cmd/{1\|2\|3}` | 主机→指定从机 | 0 | 单播命令 |
| `xiaozhi/iot/resp/{mac}` | 从机→主机 | 1 | 命令响应 |
| `xiaozhi/iot/heartbeat/{mac}` | 从机→主机 | 0 | 心跳(30秒) |
| `xiaozhi/iot/announce/{mac}` | 从机→主机 | 1 | 上线公告 |
| `xiaozhi/iot/sensor/{mac}` | 从机→主机 | 0 | 传感器上报 |

### 6.2 命令码(`shared/mqtt_iot_protocol.h`)
- `IOT_CMD_SET_GPIO=0x01` / `SET_LIGHT=0x02` / `SET_RELAY=0x03`
- `IOT_CMD_SET_SERVO=0x10`
- `IOT_CMD_SET_RGB_LIGHT=0x40` / `SET_SCENE=0x41`
- `IOT_CMD_BROADCAST_ALL_OFF/ON` / `LIGHTS_OFF/ON` / `EMERGENCY`

### 6.3 场景(`IOT_SCENE_*`)
- NONE=0 / SLEEP=1 / MOVIE=2 / NIGHT=3 / FIRE=4 / RAIN=5 / AWAY=6 / HOME=7
- **没有"聚会"和"自动模式"**,设计稿里的要删掉

### 6.4 灯光效果(`IOT_LIGHT_EFFECT_*`)
- STATIC=0 / BREATHE=1 / RAINBOW=2 / WARNING=3
- **只有4种**,设计稿里的"暖白/日落/自然/冷白"是色温预设不是效果

### 6.5 心跳超时
- 60秒无 heartbeat → 楼层标记离线 + 清空运行时数据
- 断线 → 全部清空 + 重连后 `publish_discover` 重建

## 7. 设备模型(`mqtt_device_model.c`)

### 7.1 真实设备清单(11个,在 `device_model_init` 注册)
| 楼层 | 设备名 | 类型 | cmd_type | gpio_idx |
|------|--------|------|----------|----------|
| 1F | 大门 | 舵机 | SET_SERVO | 6 |
| 1F | 大厅灯 | 灯 | SET_LIGHT | 0 |
| 2F | 主卧灯(RGB) | 氛围灯 | SET_LIGHT | 0 |
| 2F | 客厅灯 | 灯 | SET_LIGHT | 1 |
| 2F | 厕所灯 | 灯 | SET_LIGHT | 2 |
| 2F | 风扇 | 继电器 | SET_RELAY | 0 |
| 2F | 二楼晾衣杆 | 舵机 | SET_SERVO | 6 |
| 3F | 阳台灯 | 灯 | SET_LIGHT | 0 |
| 3F | 左天窗 | 舵机 | SET_SERVO | 6 |
| 3F | 右天窗 | 舵机 | SET_SERVO | 7 |
| 3F | 三楼晾衣杆 | 舵机 | SET_SERVO | 8 |

### 7.2 真实传感器
- **2F**: 雨滴(rain_mv + rain_status)
- **3F**: 烟雾(smoke_mv) + 雨滴(rain_mv + rain_status) + 火灾(fire_status) + 求助(help_status)
- **没有**: 温湿度/PM2.5/光照/AQI(室内) — 这些从 Open-Meteo 拿室外数据

### 7.3 天气数据(Open-Meteo)
- 固定位置: 上海(31.2304, 121.4737)
- Forecast API: 温度/湿度/降水/天气代码/风速
- Air Quality API: PM2.5/AQI
- 成功后10分钟刷新,失败后1分钟重试
- 写入 `device_model_update_weather()` → `publish_update()`

### 7.4 更新通知机制
```
model 变更 → publish_update() → ui_event_publish(MODEL_UPDATED)
  → 50ms 内 UI_Manager_Poll → ui_events_dispatch_pending
  → 各页面 on_model_updated 回调刷新
```
**事件驱动,非轮询。`ui_event_publish` 只设标志位,中断安全。**

## 8. UI 架构

### 8.1 双层 Shell
- **外层(C++)**: `LcdDisplay` 管理显示硬件 + 侧栏(8项) + 聊天页
- **内层(C)**: `ui_manager.c` 管理7个智能家居页面,嵌入 `smart_home_page_` 容器

### 8.2 页面枚举映射
| ShellPage (C++) | ui_page_id_t (C) | create 函数 |
|-----------------|------------------|-------------|
| Chat | (无) | 内置聊天 UI(lcd_display.cc) |
| Overview | UI_PAGE_DATA | page_data_create |
| Control | UI_PAGE_CTRL | page_ctrl_create |
| Lighting | UI_PAGE_LIGHT | page_light_create |
| Scenes | UI_PAGE_SCENE | page_scene_create |
| Environment | UI_PAGE_ENV | page_env_create |
| Network | UI_PAGE_NET | page_net_create |
| Settings | UI_PAGE_SET | page_set_create |

### 8.3 页面生命周期(无泄漏)
```
create: lv_malloc(ctx) → lv_obj_create → 注册 LV_EVENT_DELETE 回调 → ui_event_subscribe
切换:   lv_obj_clean(content_parent) → 触发 LV_EVENT_DELETE → ui_event_unsubscribe → lv_free(ctx)
```

### 8.4 主题(Cloud White)
- 背景 `#F6F9FE` / 卡片 `#FFFFFF` / 主色 `#2F6BFF`
- 绿色在线 / 橙色灯光 / 红色告警 / 紫色场景
- ⚠️ `ui_theme.c` 注释还写"Dark navy",需修正

### 8.5 图标
- 全部 Font Awesome 字体图标,无 PNG/SVG
- 尺寸: 16/20/30 像素

### 8.6 国际化
- 仅中英文(`ui_i18n.c`)
- 中文用 UTF-8 hex 转义存储
- 切换发 `UI_EVENT_LANG_CHANGED` → 重建当前页

### 8.7 浮层
- `ui_welcome_popup.c`: 欢迎回家(20% 黑遮罩)
- `smart_home_alarm_ui.c`: 火警(50% 黑遮罩)
- 都挂 `lv_screen_active()` + `LV_OBJ_FLAG_FLOATING`,z-order 最顶

## 9. 任务调度

| 任务 | 核心 | 优先级 | 栈 | 职责 |
|------|------|--------|-----|------|
| LVGL | 默认(~5) | 默认 | ~6K | UI 渲染 + 事件分发 |
| smart_home_mqtt | Core1 | 5 | 8K | MQTT 轮询 + 天气 |
| network_start | 默认 | 默认 | - | WiFi 连接 |
| 小智语音 | - | - | - | 音频收发 |

**LVGL 和 MQTT 通过事件位掩码异步解耦,MQTT 不直接操作 LVGL。**

## 10. 已知问题(待修复)

### 10.1 UI 问题
- 所有页面用 `lv_obj_set_pos` 硬编码坐标,无 flex
- `s_x_scale` 在宽度<960时压缩X,Y不动,换分辨率必崩
- 7个页面重复写 `panel/label/status_dot`,没抽 `ui_kit`
- `ui_theme.c` 注释"Dark navy"与实际浅色不符
- 离线用红字,关灯用红字,告警也红字 — 区分不出来
- i18n 多处截断/错位:"吸"(应为"呼吸")/"亮"(应为"观影")/"灯顶灯"/"输出"
- 聊天页完全空白,没接入 smart_home/ui
- 侧栏8项,设计稿要9项(缺"唤醒")

### 10.2 灯光页(最严重)
- 没用 `lv_arc` 画色环,用色块堆
- 效果按钮只有3个(少1个),"呼吸"被截成"吸"
- 标题"三楼"应为"二楼","灯顶灯"应为"灯带"

### 10.3 环境页
- 无数据时"无数据"占满整卡,信息密度低
- 设计稿的"光照320lux"是假的,没有这个传感器
- 室内/室外数据混在一起

### 10.4 网络就绪判断
- `wifi_manager.cc` 是 STUB,`is_connected()` 永远 true
- `weather_service` 和 `xiaozhi_mqtt` 的前置检查无效
- 依赖任务层轮询重试兜底

## 11. 构建与验证

### 11.1 构建
```bash
# P4 主机
idf.py set-target esp32p4
idf.py build

# 从机(三个独立项目)
cd slave/xiaozhi_slave_Firstfloor && idf.py set-target esp32s3 && idf.py build
cd slave/xiaozhi_slave_Secondfloor && idf.py set-target esp32s3 && idf.py build
cd slave/xiaozhi_slave_Thirdfloor && idf.py set-target esp32s3 && idf.py build
```

### 11.2 关键日志
- ESP-Host 就绪: `transport: Base transport is set-up`
- WiFi 连接: `NetworkEvent::Connected`
- MQTT 连接: `MQTT connected` (xiaozhi_mqtt)
- 从机 heartbeat: `Floor X heartbeat received`
- 天气成功: `weather_valid=true`

### 11.3 验证清单
- [ ] `idf.py build` 通过
- [ ] ESP-Host 连 WiFi 成功
- [ ] 小智激活(OTA)成功
- [ ] 智能家居 MQTT 连上 `8.134.167.240:1883`
- [ ] 从机 heartbeat 能收到
- [ ] 天气 API 能拉到数据
- [ ] 1024×600 UI 无重叠/越界
- [ ] 中英文切换正常
- [ ] 弹窗(欢迎/火警)不冲突

## 12. 修改原则

1. **不破坏网络栈**: 所有网络调用经 `Board::GetInstance().GetNetwork()`,不直接操作 WiFi/MQTT
2. **不破坏小智**: 不改 `Application`/`MqttProtocol`/`WebsocketProtocol`/`SetEmotion`/`SetChatMessage` 调用链
3. **不改从机协议**: `shared/mqtt_iot_protocol.h` 的数据包结构不变,从机无需重编
4. **UI 改动限 C 层**: 页面改动只在 `main/smart_home/ui/pages/`,不动 C++ 侧(除聊天页接入)
5. **事件驱动**: model 变更经 `publish_update()` → `ui_event_publish`,不直接调 LVGL
6. **内存安全**: 页面用 `lv_malloc`/`lv_free` + `LV_EVENT_DELETE` 回调,切换时 `lv_obj_clean`
7. **不伪造数据**: 无数据时显示"暂无数据",不显示假数
8. **轻资产**: 不引入大尺寸 PNG/插画,用 Font Awesome 字体图标 + LVGL 矢量绘制

## 13. 设计稿纠错(艺术稿,不能照抄)

| 设计稿 | 真实 |
|--------|------|
| 一楼:大门/客厅灯/空调 | 大门/大厅灯(客厅灯在二楼,空调不存在) |
| 二楼:窗帘/新风/摄像头/插座/扫地/热水器 | 客厅灯/厕所灯/风扇/晾衣杆(+主卧RGB在灯光页) |
| 三楼:3个SERVO | 阳台灯/左天窗/右天窗/晾衣杆 |
| 灯光:三楼WS2812B | 二楼主卧WS2812B |
| 灯光:暖白/日落/自然/冷白/夜灯 | 效果只有4种:静态/呼吸/彩虹/警示 |
| 场景:聚会/自动模式 | 协议无此场景,删掉 |
| 环境:光照320lux | 无光照传感器,删掉 |
| 环境:室内温湿度/PM2.5/AQI | 室内无此传感器,这些是室外Open-Meteo数据 |
| 设置:MQTT `10.1.1.50:1883` | 实际 `8.134.167.240:1883` |
| 总览:1F 18设备/2F 18/3F 16 | 实际 1F 2个/2F 6个/3F 4个 |
