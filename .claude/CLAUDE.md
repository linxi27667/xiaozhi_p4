# xiaozhi-for-p4 项目说明

## 项目定位

ESP32-P4 多媒体/AIoT 中控 + 三套 ESP32-S3 楼层从机。P4 运行小智音频协议、MCP、LVGL、ESP-DL 人脸识别和智能家居规则；S3 通过 MQTT 执行灯、继电器、舵机并上报传感器与心跳。

## 当前目标与构建

- ESP-IDF 目标：`esp32p4`
- 板型：`ESP_P4_FUNCTION_EV_BOARD`
- Flash：16 MB，当前使用 `partitions/v2/16m.csv`
- UI：LVGL 9.4，1024×600 MIPI-DSI
- 网络：ESP-Hosted + ESP Wi-Fi Remote，SDIO host
- 视觉：MIPI-CSI + `human_face_recognition`/ESP-DL

## 关键目录

- `main/application.cc`：主事件循环、状态机、音频和协议融合
- `main/smart_home/`：MQTT、MCP、设备模型、规则、UI、人脸
- `main/boards/esp-p4-function-ev-board/`：P4 板级初始化
- `shared/mqtt_iot_protocol.h`：P4/S3 共享线协议
- `slave/`：三套楼层从机工程

## 修改约束

- 保留用户已有未提交修改，不覆盖无关变更。
- MQTT 协议结构变化必须同步检查 P4、三套 S3 和版本兼容。
- MQTT/网络回调不要直接操作 LVGL；通过模型、事件或 `Application::Schedule()` 切换上下文。
- P4 没有片上 Wi-Fi，网络问题需同时检查 ESP-Hosted/Wi-Fi Remote。
- 当前分区表是单 factory App，不应假定存在 OTA 双槽。
- 人脸识别当前为 P4 本地 ESP-DL，不是云端识别。

## 演示城市配置

- 当前演示城市：厦门（`Xiamen`）。
- Open-Meteo 坐标：`24.4798, 118.0894`，时区：`Asia/Shanghai`。
- 城市名与坐标统一维护在 `main/smart_home/services/weather_service.h`。
- 天气卡资源名统一为 `weather_xiamen.png`，修改城市时需同步 UI、嵌入资源和中文字库。

## 设置面板硬件控制

- 设置页通过 `smart_home/services/ui_device_settings` 桥接 C UI 与 C++ 板级驱动。
- 屏幕亮度范围为 10%～100%，拖动时实时预览，松手后写入 NVS。
- 扬声器音量范围为 0%～100%，松手后写入 ES8311 并保存到 NVS。

## 人脸登录演示模式

- 登录页仅初始化 ESP-DL 人脸检测器，不加载特征识别模型或人脸数据库。
- ESP-DL 检测器连续两次返回人脸后发布登录成功事件，不额外限制置信度、尺寸或画面位置。
- 任意一次未检测到人脸都会清零连续计数。
- 摄像头取帧由互斥锁串行化，流状态使用原子变量；RGB565 帧会校验缓冲索引、有效长度和行跨度，不完整帧最多重试三次，缓冲归还失败立即终止本次采集。
- 该模式用于演示，不提供身份认证或活体检测能力。

## 常用验证

```powershell
.\idf.cmd build
```

从机是独立 ESP-IDF 工程，应分别进入各 `slave/xiaozhi_slave_*` 目录构建。
