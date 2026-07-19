# 修复计划：UI 帧率低 + 文字缺失

## 问题诊断

### 问题1：UI 帧率极低/卡顿

**根因分析**（按影响程度排序）：

1. **PNG 实时解码是最大瓶颈**：`ui_asset_image_create()` 将原始 PNG 字节存入 PSRAM，设置 `cf=LV_COLOR_FORMAT_UNKNOWN`。LVGL 每次 render 时调用 lodepng 软解码 → 输出 ARGB8888 → 再转 RGB565 显示。一张 220×220 色环 PNG 解码需要 ~193KB ARGB8888，纯 CPU 软解极慢。虽然 image cache(2MB) 会缓存解码结果，但页面切换时 cache 驱逐导致反复解码。

2. **SD 卡 1-bit 400kHz 模式**：`esp-p4-function-ev-board.cc:203` 设置 `host.max_freq_khz = SDMMC_FREQ_PROBING`(仅 400kHz)，理论吞吐 ~50KB/s。13 个 PNG 资产(~40KB)加载需近 1 秒，且 DMA 读取频繁报错。这主要影响启动速度，但也可能在页面切换重新加载时卡顿。

3. **LVGL 单缓冲 + 无撕裂防护**：`lcd_display.cc:272-293` 配置 `double_buffer=false` + `avoid_tearing=false`，渲染和刷新共享同一缓冲区，无法并行。

4. **LVGL 任务优先级低**：`lcd_display.cc:147` 设置 `task_priority=1`，低于 MQTT 任务(优先级 5)，UI 渲染可能被抢占。

### 问题2：页面文字缺失/显示方块

**根因**：中文字体字符集不足。`ui_font_cn_16/20/30.c` 使用 `puhui-common.ttf` 预生成，仅包含约 200 个汉字。UI 中大量使用的汉字不在字符集中，fallback 到 `lv_font_montserrat_14`（纯拉丁字体），导致中文显示为方块。

**缺失字符清单**（从页面代码中提取）：

| 缺失字 | 所在词 | 页面 |
|---------|--------|------|
| 厦 | 厦门 | page_data, page_set |
| 门 | 厦门 | page_data, page_set |
| 室 | 室外 | page_data |
| 步 | 同步 | page_data |
| 降 | 降水 | page_data |
| 布 | 分布 | page_data |
| 全 | 安全/全屋 | page_data, page_scene |
| 状 | 状态 | page_data |
| 联 | 联机 | page_data |
| 赋 | 赋能 | page_data |
| 计 | 设计 | page_data |
| 快 | 快捷 | page_data, page_scene |
| 观 | 观影 | page_data, page_scene |
| 暖 | 暖光 | page_scene |
| 低 | 低亮 | page_scene |
| 影 | 观影/影院 | page_scene |
| 院 | 影院 | page_scene |
| 蓝 | 影院蓝 | page_scene |
| 橙 | 橙色 | page_scene |
| 弱 | 弱光 | page_scene |
| 屋 | 全屋 | page_scene |
| 捷 | 快捷 | page_scene |
| 颜 | 颜色 | page_light |
| 呼 | 呼吸 | page_light |
| 定 | 自定义 | page_light |
| 义 | 自定义 | page_light |
| 位 | 位置 | page_set |
| 监 | 监测 | page_env |
| 测 | 监测 | page_env |
| 雪 | 有雪 | page_data |
| 雷 | 雷雨 | page_data |
| 晴 | 晴 | page_data |
| 色 | 颜色/橙色 | page_light, page_scene |

> 注：`色` 字需要确认是否在字体中。以上为初步分析，最终需要完整扫描所有页面代码确认。

---

## 修复方案

### 修复1：提升帧率 — PNG 预解码 + 显示优化

#### 1.1 PNG 预解码为 ARGB8888（核心优化）

**文件**：`main/smart_home/ui/services/ui_asset_service.c`

**改动**：在 `ui_asset_image_create()` 中，加载 PNG 后立即调用 LVGL 解码器将其解码为 ARGB8888 位图，然后释放原始 PNG 数据。这样：
- 渲染时无需再调用 lodepng 解码
- `lv_image_dsc_t` 直接指向已解码的 ARGB8888 数据
- 消除渲染时的 CPU 密集解码操作

**具体实现**：
```c
// 在 read_asset_file + asset_is_png 验证后：
// 1. 创建临时 dsc 用于解码
lv_image_dsc_t tmp_dsc;
tmp_dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN;
tmp_dsc.data = png_data;
tmp_dsc.data_size = data_size;

// 2. 调用 LVGL 解码器
lv_image_decoder_dsc_t decoder_dsc;
if (lv_image_decoder_open(&decoder_dsc, &tmp_dsc, NULL) != LV_RESULT_OK) {
    // 解码失败，回退到原始方式
}

// 3. 获取解码后的数据
uint32_t decoded_size = decoder_dsc.decoded->data_size;
const uint8_t *decoded_data = decoder_dsc.decoded->data;

// 4. 复制解码数据到 PSRAM
uint8_t *argb_data = asset_malloc(decoded_size);
memcpy(argb_data, decoded_data, decoded_size);

// 5. 释放原始 PNG 数据
heap_caps_free(png_data);

// 6. 关闭解码器
lv_image_decoder_close(&decoder_dsc);

// 7. 设置 payload 为已解码的 ARGB8888
payload->data = argb_data;
payload->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
payload->dsc.header.w = decoder_dsc.decoded->header.w;
payload->dsc.header.h = decoder_dsc.decoded->header.h;
payload->dsc.data = argb_data;
payload->dsc.data_size = decoded_size;
```

**注意**：解码后的 ARGB8888 数据比原始 PNG 大（如 220×220 = 193KB vs 原始 ~20KB），但 ESP32-P4 有 32MB PSRAM，完全可以承受。13 个 PNG 全部解码约 500KB-1MB。

#### 1.2 必须保留：TF 卡可靠挂载基线

**结论**：当前 UI 图片资源链路的第一优先级是稳定挂载 `/sdcard`，并稳定读取 `/sdcard/xiaozhi_ui/*.png`。不要为了帧率直接把 TF 卡默认改成 4-bit 40MHz，也不要退回只调用 `bsp_sdcard_mount()` 后不做读写验证的方式。

**可靠挂载方法**：

1. 在挂载 TF 卡前先打开供电：
   - LDO 使用 `LDO_VO4`，通道号 `4`，电压 `3300mV`。
   - GPIO45 是 `SD_PWRn`，低电平使能，所以必须配置为推挽输出并拉低。
   - 拉低 GPIO45 后延时约 `50ms`，等待 TF 卡供电稳定。

2. TF 卡槽使用 ESP32-P4 Function EV Board 默认 SDMMC slot0 引脚：
   - `CLK = GPIO43`
   - `CMD = GPIO44`
   - `D0 = GPIO39`
   - `D1 = GPIO40`
   - `D2 = GPIO41`
   - `D3 = GPIO42`
   - 电源使能 `SD_PWRn = GPIO45`

3. 当前稳定基线使用：
   - `SDMMC_HOST_SLOT_0`
   - `1-bit` 总线
   - `SDMMC_FREQ_PROBING` 低速探测频率
   - mount point 固定为 `/sdcard`
   - UI 图片目录固定为 `/sdcard/xiaozhi_ui`

4. `esp_vfs_fat_sdmmc_mount()` 返回 `ESP_OK` 后仍然必须做读写验证：
   - 写入 `/sdcard/test.txt`
   - 再读取并比对内容
   - 读写失败要视为“假挂载”，必须卸载并打印明确错误。

5. 启动阶段挂载失败时允许一次 BSP fallback：
   - 自定义挂载失败后，先释放自定义路径占用的 slot0/LDO。
   - 再调用 `bsp_sdcard_mount()` 做备用挂载。
   - BSP fallback 成功后仍然必须复用 `/sdcard/test.txt` 写读验证。

6. UI 资源服务允许一次按需补挂载：
   - 如果切到智能家居页面时 `/sdcard` 不存在，`ui_asset_service` 可以调用 `bsp_sdcard_mount()` 重试一次。
   - 这只是兜底，不替代启动阶段挂载日志。
   - 如果补挂载失败，日志应明确打印 `TF card retry mount failed`。

7. 挂载成功后还要检查资源目录：
   - 正确 TF 卡结构是：`TF卡根目录/xiaozhi_ui/*.png`
   - 运行时路径应是：`/sdcard/xiaozhi_ui/*.png`
   - 如果日志出现 `ROOT DIR MISSING: /sdcard/xiaozhi_ui`，优先看 `/sdcard` 根目录列表，确认是不是复制成了 `tf_card_assets/xiaozhi_ui` 或 `xiaozhi_ui/xiaozhi_ui`。

8. ESP-Hosted Wi-Fi 也使用 SDIO，但它走 slot1：
   - Wi-Fi SDIO 日志中的 `CLK[18] CMD[19] D0[14] D1[15] D2[16] D3[17]` 属于从机通信。
   - TF 卡使用 slot0，不能被 Wi-Fi SDIO 失败路径全局反初始化。
   - ESP-Hosted 的失败清理必须使用 `sdmmc_host_deinit_slot(slot1)`，不要调用全局 `sdmmc_host_deinit()`。

9. UI PNG 必须在启动早期预加载到 PSRAM：
   - 完整日志已证明：TF 卡启动时能成功挂载，`/sdcard/xiaozhi_ui` 也能列出 PNG。
   - 但 ESP-Hosted Wi-Fi SDIO slot1 启动后，UI 页面再频繁访问 TF 卡 slot0，会出现 `sdmmc_read_sectors_dma timeout 0x107`。
   - 因此页面渲染阶段不能再依赖 TF 卡实时 `stat/read`。
   - 正确流程是：`InitializeSdCard()` 成功后、网络任务启动前，调用 `ui_asset_service_preload_required()` 将 PNG 原始数据读入 PSRAM。
   - 页面创建时只从 PSRAM 缓存复制 PNG 数据并交给 LVGL 解码，不再访问 `/sdcard`。

**期望日志**：

```text
Initializing TF card
TF card power enabled: GPIO45(SD_PWRn)=0, LDO_VO4=3300mV
Mounting TF card: SDMMC slot0 1-bit ...
TF card read/write verification passed
TF card mounted and verified successfully
TF asset preload complete: 18/18 files
UI_ASSET: /sdcard mount point OK
UI_ASSET: Using preloaded TF assets; runtime UI will not read TF card
```

**禁止作为默认改动**：

- 不要把 `host.flags` 直接默认改成 `SDMMC_HOST_FLAG_4BIT`。
- 不要把默认频率直接改成 `SDMMC_FREQ_HIGHSPEED`。
- 不要只检查 `esp_vfs_fat_sdmmc_mount()` 返回值就打印“挂载成功”。
- 不要把 UI 资源路径改成 `/sdcard/tf_card_assets/xiaozhi_ui`；TF 卡上应该复制的是内层 `xiaozhi_ui` 文件夹。
- 不要在页面创建阶段对 TF 卡做大量 `stat/read`；ESP-Hosted SDIO 运行后，这会重新触发 slot0 读卡 timeout。

#### 1.3 SD 卡提速实验（只在稳定挂载后分阶段验证）

**文件**：`main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc`

**改动方向**：
```cpp
// 修改前：
host.flags &= ~(SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DDR);
host.flags |= SDMMC_HOST_FLAG_1BIT;
host.max_freq_khz = SDMMC_FREQ_PROBING;  // 400kHz

// 修改后：
host.flags &= ~SDMMC_HOST_FLAG_8BIT;  // 保留 4-bit
host.flags &= ~SDMMC_HOST_FLAG_DDR;
host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 40MHz（SDMMC_FREQ_HIGHSPEED=40000）
// slot_config.width = 4;  // 启用 4-bit 模式
```

**注意**：需要确认 P4 EV 板的 SD 卡座 D1-D3 引脚是否已正确连接。当前代码已配置了 D1-D3 引脚号（D1=10/15, D2=9/16, D3=8/17），但 `slot_config.width = 1` 限制为 1-bit。改为 4-bit 需要确保硬件支持。

**安全降级**：如果 4-bit 不稳定，至少将 1-bit 模式的频率从 400kHz 提升到 20MHz（`SDMMC_FREQ_DEFAULT`），吞吐提升 50 倍。

**实施要求**：提速必须单独提交、单独上机验证。先验证 `1-bit + SDMMC_FREQ_DEFAULT`，连续重启和页面切换都能稳定显示图片后，才允许实验 `4-bit + SDMMC_FREQ_HIGHSPEED`。如果出现 `ROOT DIR MISSING`、`sdmmc_read_sectors_dma timeout` 或 UI 图片丢失，立即回退到 1.2 的稳定基线。

#### 1.4 启用双缓冲（可选，需验证内存）

**文件**：`main/display/lcd_display.cc`

**改动**：将 `double_buffer` 改为 `true`，并将 `buff_spiram` 改为 `true`（使用 PSRAM 作为渲染缓冲区）。

```cpp
const lvgl_port_display_cfg_t disp_cfg = {
    .buffer_size = static_cast<uint32_t>(width_ * 50),  // 保持 50 行
    .double_buffer = true,       // 启用双缓冲
    .flags = {
        .buff_dma = true,
        .buff_spiram = true,     // 使用 PSRAM（双缓冲需要 ~200KB，内部 RAM 不够）
    },
};
```

**风险**：`buff_dma=true` + `buff_spiram=true` 在 ESP32-P4 上需要 PSRAM DMA 支持。如果 P4 的 MIPI-DSI 控制器不支持从 PSRAM 直接 DMA，则需要 `buff_dma=true, buff_spiram=false`（仅用内部 RAM），但双缓冲需要 ~200KB 内部 RAM，可能不够。

**建议**：先做 1.1，并保留 1.2 的 TF 卡稳定挂载基线。如果图片读取稳定但页面切换仍慢，再按 1.3 单独实验 SD 卡提速；如果仍不满意，再尝试双缓冲。

---

### 修复2：补全中文字体字符集

#### 2.1 收集所有缺失字符

扫描所有页面代码（page_data.c, page_ctrl.c, page_light.c, page_scene.c, page_env.c, page_net.c, page_set.c, smart_home_alarm_ui.c, ui_welcome_popup.c, ui_manager.c）中的中文 UTF-8 字符，与现有字体字符集对比，生成完整的缺失字符列表。

#### 2.2 重新生成字体文件

使用 `lv_font_conv` 工具，基于 `puhui-common.ttf`，将现有字符集 + 缺失字符合并，重新生成三个字体文件：

```bash
# 示例命令（需要安装 lv_font_conv）
lv_font_conv --font puhui-common.ttf \
  -r 0x20-0x7E \
  --symbols "一三上下与个中为主义二于亮从他件优传伸体使供信值偏像光入关其冲出击函分切初到制前功加动化区卧厂厅厕参取变口台合同名向启吸告和唤器回固图土在场块型基堆境壤声处备复多大天失头好始字安定实客家宽密将小层居屏己已帘带帧常幕幹度庭廊延建开式引当待态总恢息感慧成或户所扇手打扫找报择持指换据排接控描提搜摘支收改放效敏数文断新无早时明显晚景智晾有未本机杆来板构架染根格桢档楼模正步气流消添温渲湿源滴火灯点烟焰照片版特状率环现理用的省知码确磁示禁离种称空窗第等简类系素索约线组给络统缓编网置者聊能自节芯菜行衣要览言警认让设语调败质走起路转输过近返这进连迟适选透逐递通速配醒释重量针链长门闭阳附际院雨雾需静面音顶预风高厦门室步降布全状联赋计快观暖低影院蓝橙弱屋捷颜色呼定义位监测雪雷晴色" \
  --size 16 --bpp 4 --format lvgl --lv-include lvgl.h --lv-font-name ui_font_cn_16 \
  --no-compress -o ui_font_cn_16.c

# 同理生成 20px 和 30px 版本
```

**新增字符汇总**：厦门室步降布全状联赋计快观暖低影院蓝橙弱屋捷颜色呼定义位监测雪雷晴色（约 35 个）

#### 2.3 替换字体文件

将重新生成的 `ui_font_cn_16.c`、`ui_font_cn_20.c`、`ui_font_cn_30.c` 替换到 `main/smart_home/ui/fonts/` 目录。

**注意**：新增约 35 个汉字，每个汉字在 16px/4bpp 下约 128 字节，三个尺寸总计增加约 35 × (128+200+450) ≈ 27KB flash 空间，完全可接受。

---

## 实施步骤

### Step 1：PNG 预解码优化（帧率修复核心）
1. 修改 `ui_asset_service.c` 的 `ui_asset_image_create()` 函数
2. 加载 PNG 后立即解码为 ARGB8888，释放原始 PNG 数据
3. 设置 `cf = LV_COLOR_FORMAT_ARGB8888` 而非 `LV_COLOR_FORMAT_UNKNOWN`
4. 测试验证：所有 PNG 图片正常显示，页面切换流畅度明显改善

### Step 2：TF 卡挂载稳定性保护
1. 保留 `esp-p4-function-ev-board.cc` 中的 LDO_VO4、GPIO45、slot0、1-bit 低速挂载流程
2. 保留 `/sdcard/test.txt` 写入和读取验证，防止 `esp_vfs_fat_sdmmc_mount()` 假成功
3. 保留 `/sdcard/xiaozhi_ui` 目录检查和 `/sdcard` 根目录诊断日志
4. 只有在上机稳定显示图片后，才允许另起改动尝试 1-bit @ 20MHz 或 4-bit @ 40MHz

### Step 3：补全中文字体
1. 完整扫描所有页面代码，收集全部缺失汉字
2. 使用 `lv_font_conv` 重新生成三个尺寸的字体文件
3. 替换字体文件，编译验证
4. 验证：所有页面中文文字正常显示，无方块

### Step 4（可选）：双缓冲优化
1. 如果 Step 1-3 后帧率仍不满意，尝试启用双缓冲
2. 需要验证 PSRAM DMA 在 MIPI-DSI 场景下是否可用
3. 如果不可用，考虑增大单缓冲行数（如 100 行）

---

## 验证标准

1. **帧率**：页面切换和动画无明显卡顿，LVGL render time < 30ms/帧
2. **文字**：所有页面中文文字完整显示，无方块/空白
3. **图片**：所有 PNG 资产正常显示，无闪烁/错位
4. **内存**：PSRAM 使用增量 < 2MB，内部 RAM 不溢出
5. **稳定性**：连续运行 10 分钟无 crash/看门狗重启

## 假设与决策

- **假设**：ESP32-P4 的 32MB PSRAM 足以存储所有解码后的 PNG 数据（预估 ~1MB）
- **假设**：`puhui-common.ttf` 字体文件包含所有需要的中文字符
- **决策**：优先保证 TF 卡稳定挂载和 PNG 资源可读；PNG 预解码用于提升帧率，SD 卡提速只能作为稳定后的独立实验，双缓冲最后
- **决策**：字体修复采用重新生成字符集方案，而非启用 Tiny TTF（避免运行时 PSRAM 开销和 CPU 占用）
