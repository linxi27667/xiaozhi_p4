# TF 卡 PNG 资产读取说明

本文档说明 ESP32-P4 智能家居 UI 如何从 TF 卡读取 PNG，并把图片替换到 LVGL 界面里。

---

## 问题修复总结(2026-06-21)

本次共修复两个关联问题,均围绕 SDMMC 控制器在 ESP32-P4 上的使用。

### 问题一:看门狗复位崩溃(WDT reset crash)

**现象**:系统启动后立即触发 task watchdog reset,无法进入主循环。

**根因分析**:
- 板级代码在 SD 卡挂载失败后调用了 `sdmmc_host_deinit()`(全主机清理)
- ESP32-P4 的 SDMMC 控制器被两个 slot 共享:
  - slot 0:TF 卡(1-bit,CLK=43/CMD=44/D0=39)
  - slot 1:esp-hosted SDIO(4-bit,40MHz,CLK=18/CMD=19/D0-D3=14-17,连 ESP32-C6 WiFi 协处理器)
- esp-hosted 组件用 `__attribute__((constructor))` 在 `app_main()` 之前就初始化了 slot 1
- 调用 `sdmmc_host_deinit()` 会把整个 SDMMC 主机控制器清掉,导致 esp-hosted 的 slot 1 也失效
- WiFi 协处理器失联后,网络任务死等,触发看门狗

**修复方法**:
- 删除板级代码中所有 `sdmmc_host_deinit()` 和 `sdmmc_host_deinit_slot()` 调用
- 让 `esp_vfs_fat_sdmmc_mount()` 内部的 `call_host_deinit()` 自行处理失败清理
- 关键认知:`SDMMC_HOST_DEFAULT()` 包含 `SDMMC_HOST_FLAG_DEINIT_ARG` 标志,失败时只调 `sdmmc_host_deinit_slot(slot)`(slot 级清理),不会动 slot 1

**修改文件**:`main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc`

### 问题二:TF 卡无法读取(SD card mount failure)

**现象**:系统正常启动(WiFi/MQTT/OTA/天气全部正常),但 TF 卡挂载失败,所有 PNG 资产无法加载,UI 全部回退到 Font Awesome 图标占位符。

**根因分析**(深度检索 ESP-IDF v5.5.3 驱动源码):

1. **上电延时不足**:
   - 原代码 `InitializeSdPower()` 上电后只延时 50ms
   - SD 规范要求至少 74 个时钟周期 + 1ms,但实测部分 SD 卡需要 250ms 以上才能响应 ACMD41
   - 启动日志显示渐进式失败:
     - 尝试 1(50ms 后):ACMD41 TIMEOUT(卡未就绪)
     - 尝试 2(165ms 后):CMD8 CRC(卡开始响应但信号不稳)
     - 尝试 3(278ms 后):SEND_SCR TIMEOUT(卡响应大部分命令但数据传输失败)
   - 每次重试都更深一层,说明卡需要更多稳定时间

2. **探测频率过快**:
   - BSP 默认用 `SDMMC_FREQ_DEFAULT`(20MHz)
   - SD 卡初始化阶段应使用 400kHz 探测频率(SD 规范要求)
   - 20MHz 对 marginal SD 卡太快,导致信号完整性问题

3. **无重试机制**:
   - 单次挂载失败就放弃,没有给 SD 卡第二次机会

**修复方法**(三处改动):

1. **增加上电延时**:`InitializeSdPower()` 延时从 50ms → 500ms
   ```cpp
   vTaskDelay(pdMS_TO_TICKS(500));  // 原来是 50ms
   ```

2. **添加重试循环**:`MountSdCard()` 增加 3 次重试,每次失败后等 500ms
   ```cpp
   const int kMaxRetries = 3;
   for (int attempt = 1; attempt <= kMaxRetries; ++attempt) {
       ret = esp_vfs_fat_sdmmc_mount(...);
       if (ret == ESP_OK) return ESP_OK;
       vTaskDelay(pdMS_TO_TICKS(500));
   }
   ```

3. **所有挂载路径改用 400kHz 探测频率**:
   - `MountSdCard()` 主路径:`host.max_freq_khz = SDMMC_FREQ_PROBING`
   - `InitializeSdCard()` BSP 回退路径:`bsp_host.max_freq_khz = SDMMC_FREQ_PROBING`
   - `ui_asset_service.c` 第三次重试路径:`retry_host.max_freq_khz = SDMMC_FREQ_PROBING`

**修改文件**:
- `main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc`(3 处)
- `main/smart_home/ui/services/ui_asset_service.c`(2 处 + 头文件包含)

**关键认知**:
- `SDMMC_HOST_FLAG_DEINIT_ARG` 标志确保 `call_host_deinit()` 只清理 slot 0,不影响 esp-hosted 的 slot 1
- ESP32-P4 `SDMMC_LL_SLOT_SUPPORT_GPIO_MATRIX(0) = 0`,slot 0 只支持 IOMUX,板子引脚(CLK=43/CMD=44/D0=39)正好匹配 IOMUX 默认引脚
- IOMUX 模式下 `sdmmc_host_pullup_en_internal()` 设置的上拉电阻会被保留(GPIO matrix 模式下 `gpio_reset_pin()` 会清除上拉)

### 问题三:PNG 占位符未替换为实际图片

**现象**:TF 卡挂载成功后,部分 PNG 资产仍未被代码引用,UI 一直显示 Font Awesome 图标占位符。

**深度检索结果**:
- `s_required_assets[]` 中声明了 18 个 PNG,但只有 13 个被代码引用
- 5 个未引用的 PNG:`alarm_siren.png`、`scene_rain.png`、`scene_fire.png`、`weather_cloud.png`、`logo_robot.png`

**修复方法**(接线未使用的 PNG):

1. **火警弹窗接入 `alarm_siren.png`**:
   - 文件:`main/smart_home/ui/core/smart_home_alarm_ui.c`
   - 原来:直接用 `ICON_FIRE` 图标
   - 现在:优先 `ui_asset_image_create(header, "alarm_siren.png")`,失败回退到 `ICON_FIRE`

2. **场景页新增 FIRE 和 RAIN 场景**:
   - 文件:`main/smart_home/ui/pages/page_scene.c`
   - 协议中 `IOT_SCENE_FIRE=4`、`IOT_SCENE_RAIN=5` 已定义,但 UI 未实现
   - 新增两个场景卡片:
     - 火灾场景:用 `scene_fire.png`,红色 `0xC62828`,回退 `ICON_FIRE`
     - 雨天场景:用 `scene_rain.png`,蓝色 `0x3B82F6`,回退 `ICON_WATER`(=FONT_AWESOME_CLOUD_RAIN)

**未接线的 3 个 PNG**(保留在 s_required_assets 中,暂不使用):
- `weather_cloud.png`:天气卡片已用 `weather_guangzhou.png`,无需替换
- `logo_robot.png`:无对应 UI 位置,保留备用
- `color_wheel_220.png`:已在 page_light.c 使用,无需改动

### 验证

- `idf.py build` 编译通过(2026-06-21)
- 固件大小:0x3b0320 字节(3.87MB),分区剩余 6%
- 需要用户烧录后实测 TF 卡挂载和 PNG 显示

### 修改文件清单

| 文件 | 改动 |
|------|------|
| `main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc` | 上电延时 50ms→500ms;3 次重试循环;BSP 回退改 400kHz |
| `main/smart_home/ui/services/ui_asset_service.c` | 第三次重试改 400kHz + 500ms 延时;新增 sdmmc 头文件 |
| `main/smart_home/ui/core/smart_home_alarm_ui.c` | 火警弹窗接入 alarm_siren.png |
| `main/smart_home/ui/pages/page_scene.c` | 新增 FIRE/RAIN 场景卡片 |

---

## 目录必须这样放

TF 卡挂载点固定为：

```text
/sdcard
```

UI 图片根目录固定为：

```text
/sdcard/xiaozhi_ui
```

所以 TF 卡根目录里必须直接有 `xiaozhi_ui` 文件夹，不能多套一层。

正确结构：

```text
TF 卡根目录/
└── xiaozhi_ui/
    ├── siyin_logo.png
    ├── overview_home.png
    ├── weather_guangzhou.png
    ├── floor_1.png
    ├── floor_2.png
    ├── floor_3.png
    ├── scene_lights.png
    ├── scene_home.png
    ├── scene_away.png
    ├── scene_sleep.png
    ├── scene_movie.png
    ├── scene_night.png
    └── color_wheel_220.png
```

错误结构：

```text
TF 卡根目录/
└── xiaozhi_ui/
    └── xiaozhi_ui/
        └── ...
```

```text
TF 卡根目录/
└── tf_card_assets/
    └── xiaozhi_ui/
        └── ...
```

仓库里的参考文件位于：

```text
tf_card_assets/xiaozhi_ui/
```

烧录或测试前，把这个目录里的 `xiaozhi_ui` 文件夹整体复制到 TF 卡根目录。

## 文件名规则

代码按固定文件名读取图片，大小写和下划线必须完全一致。

例如：

```c
ui_asset_image_create(parent, "scene_home.png");
```

实际读取路径是：

```text
/sdcard/xiaozhi_ui/scene_home.png
```

不要改成中文名，不要加空格，不要把 `.png` 改成 `.PNG`。如需新增图片，先把 PNG 放到 `tf_card_assets/xiaozhi_ui/`，再在页面代码里使用同名字符串。

## 关键配置

这些配置必须开启，否则长文件名如 `weather_guangzhou.png` 可能读不到：

```text
CONFIG_FATFS_LFN_NONE=n
CONFIG_FATFS_LFN_HEAP=y
CONFIG_FATFS_MAX_LFN=255
CONFIG_LV_USE_LODEPNG=y
```

当前项目已在 `sdkconfig` 和 `sdkconfig.defaults` 中启用长文件名与 lodepng。

## 代码读取链路

1. 板级代码挂载 TF 卡：

```text
main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc
InitializeSdCard() -> bsp_sdcard_mount()
```

2. BSP 负责 SDMMC 电源和挂载：

```text
managed_components/espressif__esp32_p4_function_ev_board/esp32_p4_function_ev_board.c
```

当前策略是 1-bit SDMMC + 20MHz，优先保证稳定读取。不要为了提速直接改成 4-bit 40MHz；如果硬件、线长或卡不稳定，挂载失败后 UI 一张图都读不到。

3. UI 注册 LVGL 文件系统盘符：

```text
main/smart_home/ui/services/ui_asset_service.h
UI_ASSET_DRIVE_LETTER = 'S'
UI_ASSET_ROOT = "/sdcard/xiaozhi_ui"
```

`S:scene_home.png` 会被映射到：

```text
/sdcard/xiaozhi_ui/scene_home.png
```

4. 页面代码创建图片：

```c
lv_obj_t *img = ui_asset_image_create(parent, "scene_home.png");
```

`ui_asset_image_create()` 会先检查文件存在，再尝试把 PNG 预解码到内存以提升帧率。预解码失败时不会直接放弃图片，而是回退到 `S:xxx.png` 文件源，让 LVGL 自己从 TF 卡解码显示。

## 日志怎么看

启动后会调用：

```c
ui_asset_service_diagnose();
```

常见日志含义：

```text
SD card mounted successfully
```

TF 卡已挂载。

```text
UI_ASSET_ROOT = /sdcard/xiaozhi_ui
Root dir exists: /sdcard/xiaozhi_ui
```

图片目录存在。

```text
required asset OK: scene_home.png
```

指定 PNG 文件存在且 PNG 头正确。

```text
ROOT DIR MISSING: /sdcard/xiaozhi_ui
```

TF 卡没挂载，或 `xiaozhi_ui` 没放在 TF 卡根目录。

```text
Found assets at wrong path: /sdcard/xiaozhi_ui/xiaozhi_ui
```

目录多套了一层，应该把里面那层 `xiaozhi_ui` 移到 TF 卡根目录。

```text
required asset MISSING: weather_guangzhou.png
```

文件缺失、文件名大小写不一致，或长文件名配置没生效。

```text
asset image pre-decoded: scene_home.png
```

PNG 已经从 TF 卡读出并预解码，性能最好。

```text
asset image using TF file source
```

文件存在，但预解码失败或直接读入失败，已回退到 LVGL 文件源显示。图片仍应能替换到界面，只是性能可能差一些。

## 修改注意事项

- 不要在 SD 挂载阶段强制写 `/sdcard/test.txt` 作为成功条件。UI 只需要读 PNG，写失败不应该导致卸载 TF 卡。
- 不要在未验证硬件稳定性的情况下把 SDMMC 改成 4-bit 40MHz。先保证 `1-bit 20MHz` 能稳定读取所有 PNG。
- 不要删除 `CONFIG_FATFS_LFN_HEAP=y`，否则长文件名资源会失败。
- 不要把 `ui_asset_image_create()` 改成只有预解码路径。预解码是优化，`S:xxx.png` 文件源回退是保底显示路径。
- 如果重新拉取或更新 `managed_components/espressif__esp32_p4_function_ev_board`，检查 BSP 里的 SD 卡验证逻辑是否又变回了写文件验证。
