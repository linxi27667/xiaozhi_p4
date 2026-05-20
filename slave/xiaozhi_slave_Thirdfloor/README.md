# IoT Slave Device - IoT从机设备

IoT 控制系统从机设备，通过 ESP-NOW 接收主机命令控制 GPIO 和舵机。

## 功能特性

- 连接WiFi路由器（与主机同一网络）
- 通过 ESP-NOW 接收主机的控制命令
- 控制 4 个 LED（可扩展为继电器等其他设备）
- 控制 2 个舵机（预留接口）
- 自动发现主机并注册
- 心跳保活机制

## 硬件配置

默认 GPIO 配置：

| GPIO | 名称 | 功能 | 说明 |
|------|------|------|------|
| GPIO2 | led1 | LED 1 | 可改为继电器 |
| GPIO4 | led2 | LED 2 | 可改为继电器 |
| GPIO5 | led3 | LED 3 | 可改为继电器 |
| GPIO18 | led4 | LED 4 | 可改为继电器 |
| GPIO19 | servo1 | 舵机 1 | PWM控制 |
| GPIO21 | servo2 | 舵机 2 | PWM控制 |

**修改方法**：在 `main/iot_slave_main.c` 中修改以下宏定义：

```c
#define LED_GPIO_1      GPIO_NUM_2
#define LED_GPIO_2      GPIO_NUM_4
#define LED_GPIO_3      GPIO_NUM_5
#define LED_GPIO_4      GPIO_NUM_18

#define SERVO_GPIO_1    GPIO_NUM_19
#define SERVO_GPIO_2    GPIO_NUM_21
```

## WiFi 配置

**重要**：从机必须连接与主机相同的WiFi路由器！

修改 `main/APP/Src/app_wifi.c`：

```c
// 在这里修改你的网络配置（与主机相同）
#define ROUTER_SSID      "你的WiFi名称"
#define ROUTER_PASSWORD  "你的WiFi密码"
#define MAX_RETRY_TIMES  5
```

## 编译和烧录

```bash
# 进入从机目录
cd xiaozhi_slave

# 编译
idf.py build

# 烧录（替换 COM3 为你的串口）
idf.py -p COM3 flash monitor
```

## 使用流程

### 1. 烧录从机代码

将代码烧录到 ESP32 从机设备。

### 2. 获取从机 MAC 地址

烧录后，查看日志，找到 MAC 地址：

```
I (xxx) IOT_SLAVE: Device MAC: AA:BB:CC:DD:EE:FF
```

### 3. 在主机上注册从机

在小智主机上，修改 `custom_esp32s3_cam.cc`：

```cpp
void InitializeIot() {
    if (Iot_Mcp_Init()) {
        ESP_LOGI(TAG, "IOT controller initialized");
        
        // 添加从机设备（替换 MAC 地址）
        uint8_t slave_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        App_IOT_Add_Remote_Device("bedroom", slave_mac, 6);  // 6个GPIO
    }
}
```

### 4. 控制从机

**语音控制LED**：
- "打开卧室的 led1"
- "关闭卧室的 led2"

**语音控制舵机**：
- "把卧室的舵机转到90度"
- "设置卧室 servo1 为45度"

## 通信协议

| 命令 | 值 | 说明 |
|------|-----|------|
| IOT_CMD_SET_GPIO | 0x01 | 设置 GPIO 输出（LED开/关） |
| IOT_CMD_GET_GPIO | 0x02 | 读取 GPIO 状态 |
| IOT_CMD_SET_SERVO | 0x10 | 设置舵机角度（0-180度） |
| IOT_CMD_DISCOVER | 0x06 | 发现设备 |
| IOT_CMD_ANNOUNCE | 0x07 | 设备公告 |
| IOT_CMD_HEARTBEAT | 0x04 | 心跳检测 |

## 项目结构

```
xiaozhi_slave/
├── CMakeLists.txt           # 项目配置
├── sdkconfig.defaults       # ESP-IDF 默认配置
├── README.md               # 本文档
└── main/
    ├── CMakeLists.txt      # 组件配置
    ├── iot_slave_main.c    # 主程序
    ├── APP/                # 应用层
    │   ├── Inc/
    │   │   └── app_wifi.h
    │   └── Src/
    │       ├── app_wifi.c  # WiFi连接逻辑
    │       └── ...
    └── BSP/                # 底层驱动
        ├── Inc/
        │   └── bsp_wifi.h
        └── Src/
            ├── bsp_wifi.c  # WiFi驱动
            └── ...
```

## 注意事项

1. **WiFi必须相同**：从机和主机必须连接同一个WiFi路由器
2. **通道匹配**：ESP-NOW 使用通道 1，确保WiFi路由器也在通道1
3. **MAC 地址**：每个从机有唯一的 MAC 地址，需要在主机上注册
4. **距离限制**：ESP-NOW 的有效距离约为 200 米（开放环境）
5. **舵机供电**：舵机需要独立5V供电，ESP32的3.3V可能不足

## 故障排除

### WiFi连接失败
- 检查SSID和密码是否正确
- 确认路由器在通道1
- 查看日志中的错误代码

### 无法接收命令
- 确认主机已注册从机MAC地址
- 检查ESP-NOW初始化日志
- 确认从机已发现主机

### 舵机不转
- 检查舵机供电是否充足
- 确认GPIO引脚连接正确
- 查看日志中的角度设置信息
