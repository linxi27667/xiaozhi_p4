I (43) hex_psram: vendor id    : 0x0d (AP)
I (43) hex_psram: Latency      : 0x01 (Fixed)
I (44) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (45) hex_psram: dev id       : 0x03 (generation 4)
I (48) hex_psram: density      : 0x07 (256 Mbit)
I (53) hex_psram: good-die     : 0x06 (Pass)
I (57) hex_psram: SRF          : 0x02 (Slow Refresh)
I (61) hex_psram: BurstType    : 0x00 ( Wrap)
I (65) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (70) hex_psram: BitMode      : 0x01 (X16 Mode)
I (74) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
I (79) hex_psram: DriveStrength: 0x00 (1/1)
I (83) MSPI Timing: Enter psram timing tuning
I esp_psram: Found 32MB PSRAM device
I esp_psram: Speed: 200MHz
I (330) mmu_psram: .rodata xip on psram
I (467) mmu_psram: .text xip on psram
I (467) hex_psram: psram CS IO is dedicated
I (468) cpu_start: Multicore app
I (477) cpu_start: GPIO 38 and 37 are used as console UART I/O pins
I (478) cpu_start: Pro cpu start user code
I (478) cpu_start: cpu freq: 360000000 Hz
I (480) app_init: Application information:
I (483) app_init: Project name:     xiaozhi
I (487) app_init: App version:      2.2.4
I (491) app_init: Compile time:     Jun 20 2026 17:10:09
I (496) app_init: ELF file SHA256:  a9a90b6d0...
I (500) app_init: ESP-IDF:          v5.5.3-dirty
I (505) efuse_init: Min chip rev:     v1.0
I (509) efuse_init: Max chip rev:     v1.99
I (513) efuse_init: Chip rev:         v1.3
I (516) heap_init: Initializing. RAM available for dynamic allocation:
I (523) heap_init: At 4FF25950 len 00015670 (85 KiB): RETENT_RAM
I (528) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
I (534) heap_init: At 4FF40000 len 00060000 (384 KiB): RAM
I (539) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
I (544) esp_psram: Adding pool of 29120K of PSRAM memory to heap allocator
I (550) esp_psram: Adding pool of 23K of PSRAM memory gap generated due to end address alignment of irom to the heap allocator
I (561) esp_psram: Adding pool of 3K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
I (573) spi_flash: detected chip: gd
I (576) spi_flash: flash io: qio
I (618) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (625) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (631) main_task: Started on CPU0
I (634) esp_psram: Reserving pool of 64K of internal memory for DMA/internal allocations
I (642) main_task: Calling app_main()
I (652) Board: UUID=bfba9048-2a61-4c2a-a7b0-1de80e6c0f4d SKU=esp-p4-function-ev-board
I (653) button: IoT Button Version: 4.1.6
W (656) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
I (672) ESP32_P4_EV: MIPI DSI PHY Powered on
I (677) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (681) ESP32_P4_EV: Install EK79007 LCD control panel
I (686) ek79007: version: 1.0.4
I (853) ESP32_P4_EV: Display initialized
I (853) Display: Power management not supported
I (853) LcdDisplay: Initialize LVGL library
I (853) LcdDisplay: Initialize LVGL port
I (857) LVGL: Starting LVGL task
I (860) LcdDisplay: Adding LCD display
I (864) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (872) GT911: TouchPad_ID:0x39,0x31,0x31
I (875) GT911: TouchPad_Config_Version:89
I (879) ESP32P4FuncEV: Touch controller registered with LVGL
I (884) ESP32P4_TF: Initializing TF card
I (938) ESP32P4_TF: SD card power enabled: GPIO45(SD_PWRn)=0, LDO_VO4=3300mV
I (938) ESP32P4_TF: Mounting TF card via SDMMC slot 0, 1-bit, 20000kHz: CLK=43 CMD=44 D0=39
I (941) sdmmc_periph: sdmmc_host_init: SDMMC host already initialized, skipping init flow
I (993) ESP32P4_TF: TF card mounted successfully at /sdcard (SDMMC slot0, 1-bit)
Name: SD
Type: SDHC
Speed: 20.00 MHz (limit: 20.00 MHz)
Size: luMB
CSD: ver=2, sector_size=512, capacity=15613952 read_bl_len=9
SSR: bus_width=1
I (1004) ESP32P4_TF: UI asset folder found: /sdcard/xiaozhi_ui
I (1008) UI_ASSET: Preloading TF UI assets from /sdcard/xiaozhi_ui before Wi-Fi SDIO runtime traffic
I (1048) UI_ASSET: preload OK: siyin_logo.png (9857 bytes)
I (1058) UI_ASSET: preload OK: overview_home.png (1838 bytes)
I (1066) UI_ASSET: preload OK: weather_guangzhou.png (1381 bytes)
I (1078) UI_ASSET: preload OK: weather_cloud.png (1511 bytes)
I (1085) UI_ASSET: preload OK: floor_1.png (981 bytes)
I (1091) UI_ASSET: preload OK: floor_2.png (999 bytes)
I (1098) UI_ASSET: preload OK: floor_3.png (1005 bytes)
I (1106) UI_ASSET: preload OK: alarm_siren.png (1428 bytes)
I (1118) UI_ASSET: preload OK: logo_robot.png (2271 bytes)
I (1126) UI_ASSET: preload OK: scene_lights.png (786 bytes)
I (1134) UI_ASSET: preload OK: scene_home.png (724 bytes)
I (1142) UI_ASSET: preload OK: scene_away.png (671 bytes)
I (1151) UI_ASSET: preload OK: scene_sleep.png (772 bytes)
I (1161) UI_ASSET: preload OK: scene_movie.png (652 bytes)
I (1170) UI_ASSET: preload OK: scene_night.png (831 bytes)
I (1180) UI_ASSET: preload OK: scene_rain.png (1485 bytes)
I (1192) UI_ASSET: preload OK: scene_fire.png (2523 bytes)
I (1258) UI_ASSET: preload OK: color_wheel_220.png (20412 bytes)
I (1258) UI_ASSET: TF asset preload complete: 18/18 files, 50127 bytes
I (1259) ESP32P4FuncEV: Initializing camera
I (1263) ESP32P4FuncEV: Camera initialized successfully via BSP
I (1268) ESP32P4FuncEV: Initializing font support
I (1273) ESP32P4FuncEV: Custom font loaded successfully: line_height=25
W (1279) ledc: GPIO 26 is not usable, maybe conflict with others
I (1285) Backlight: Set brightness to 75
I (1288) StateMachine: State: unknown -> starting
I (1337) Es8311AudioCodec: Duplex channels created
I (1342) ES8311: Work in Slave mode
I (1345) Es8311AudioCodec: Es8311AudioCodec initialized
I (1345) AudioCodec: Audio codec started
I (1347) MCP: Add tool: self.get_device_status
I (1347) MCP: Add tool: self.audio_speaker.set_volume
I (1352) MCP: Add tool: self.screen.set_brightness
I (1356) MCP: Add tool: self.screen.set_theme
I (1360) MCP: Add tool: self.get_system_info [user]
I (1365) MCP: Add tool: self.reboot [user]
I (1369) MCP: Add tool: self.upgrade_firmware [user]
I (1373) MCP: Add tool: self.screen.get_info [user]
I (1378) MCP: Add tool: self.screen.snapshot [user]
I (1383) MCP: Add tool: self.screen.preview_image [user]
I (1388) Assets: The storage free size is 65536 KB
I (1392) Assets: The partition size is 8192 KB
I (1486) Assets: The checksum calculation time is 89 ms
I (1486) MCP: Add tool: self.assets.set_download_url [user]
I (1487) MCP: Add tool: self.iot.get_status
I (1489) MCP: Add tool: self.iot.get_sensors
I (1493) MCP: Add tool: self.iot.discover
I (1497) MCP: Add tool: self.iot.set_gpio
I (1500) MCP: Add tool: self.iot.set_light
I (1504) MCP: Add tool: self.iot.set_rgb_light
I (1508) MCP: Add tool: self.iot.set_scene
I (1512) MCP: Add tool: self.iot.set_relay
I (1516) MCP: Add tool: self.iot.set_servo_by_index
I (1521) MCP: Add tool: self.iot.all_off
I (1524) MCP: Add tool: self.iot.all_on
I (1528) MCP: Add tool: self.iot.all_lights_off
I (1532) MCP: Add tool: self.iot.all_lights_on
I (1536) DEV_MODEL: Model initialized: 11 devices
I (1541) SH_MQTT: Smart-home MQTT initialized
I (1545) SmartHomeTasks: Smart home task layer started
I (1551) Application: Network start task started
I (1554) WifiManager: Initializing...
I (1558) transport: Attempt connection with slave: retry[0]
I (1563) transport: Reset slave using GPIO[54]
I (1567) os_wrapper_esp: GPIO [54] configured
I (3231) sdio_wrapper: SDIO master: Slot 1, Data-Lines: 4-bit Freq(KHz)[40000 KHz]
I (3231) sdio_wrapper: GPIOs: CLK[18] CMD[19] D0[14] D1[15] D2[16] D3[17] Slave_Reset[54]
I (3231) H_SDIO_DRV: Starting SDIO process rx task
I (3235) sdio_wrapper: Queues: Tx[20] Rx[20] SDIO-Rx-Mode[1]
Name:
Type: SDIO
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: luMB
CSD: ver=1, sector_size=0, capacity=0 read_bl_len=0
SCR: sd_spec=0, bus_width=0
TUPLE: DEVICE, size: 3: D9 01 FF
TUPLE: MANFID, size: 4
  MANF: 0092, CARD: 6666
TUPLE: FUNCID, size: 2: 0C 00
TUPLE: FUNCE, size: 4: 00 00 02 32
TUPLE: CONFIG, size: 5: 01 01 00 02 07
TUPLE: CFTABLE_ENTRY, size: 8
  INDX: C1, Intface: 1, Default: 1, Conf-Entry-Num: 1
  IF: 41
  FS: 30, misc: 0, mem_space: 1, irq: 1, io_space: 0, timing: 0, power: 0
  IR: 30, mask: 1,   IRQ: FF FF
  LEN: FFFF
TUPLE: END
I (3318) sdio_wrapper: Function 0 Blocksize: 512
I (3322) sdio_wrapper: Function 1 Blocksize: 512
I (3326) H_SDIO_DRV: SDIO Host operating in STREAMING MODE
I (3331) H_SDIO_DRV: generate slave intr
I (3337) transport: Received INIT event from ESP32 peripheral
I (3341) transport: EVENT: 12
I (3343) transport: EVENT: 11
I (3346) transport: capabilities: 0xd
I (3349) transport: Features supported are:
I (3353) transport:      * WLAN
I (3356) transport:        - HCI over SDIO
I (3359) transport:        - BLE only
I (3363) transport: EVENT: 13
I (3365) transport: ESP board type is : 13

I (3369) transport: Base transport is set-up

I (3374) transport: Slave chip Id[12]
I (3377) hci_stub_drv: Host BT Support: Disabled
I (3381) H_SDIO_DRV: Received INIT event
I (3385) rpc_wrap: --- ESP Event: Slave ESP Init ---
I (4283) WifiManager: Initialized
I (4283) WifiBoard: Starting WiFi connection attempt
I (4284) WifiManager: Starting station
I (4382) Application: Network start task finished
I (4403) rpc_req: Scan start Req

I (4413) WifiBoard: WiFi scanning
I (6832) WifiStation: No AP found, next scan in 10 seconds
I (8601) LcdDisplay: Shell page switch: overview
I (8601) UI_ASSET: LVGL asset drive S: mapped to /sdcard/xiaozhi_ui
I (8601) UI_ASSET: === TF Card Asset Diagnosis ===
I (8605) UI_ASSET: UI_ASSET_ROOT = /sdcard/xiaozhi_ui
I (8609) UI_ASSET: Drive letter = S
I (8613) UI_ASSET: Preload state: done=1 count=18 bytes=50127
I (8618) UI_ASSET: Unavailable state: 0
I (8622) UI_ASSET: Using preloaded TF assets; runtime UI will not read TF card
I (8629) UI_ASSET:   cache[0] siyin_logo.png OK (9857 bytes)
I (8634) UI_ASSET:   cache[1] overview_home.png OK (1838 bytes)
I (8640) UI_ASSET:   cache[2] weather_guangzhou.png OK (1381 bytes)
I (8646) UI_ASSET:   cache[3] weather_cloud.png OK (1511 bytes)
I (8651) UI_ASSET:   cache[4] floor_1.png OK (981 bytes)
I (8656) UI_ASSET:   cache[5] floor_2.png OK (999 bytes)
I (8661) UI_ASSET:   cache[6] floor_3.png OK (1005 bytes)
I (8666) UI_ASSET:   cache[7] alarm_siren.png OK (1428 bytes)
I (8672) UI_ASSET:   cache[8] logo_robot.png OK (2271 bytes)
I (8677) UI_ASSET:   cache[9] scene_lights.png OK (786 bytes)
I (8683) UI_ASSET:   cache[10] scene_home.png OK (724 bytes)
I (8688) UI_ASSET:   cache[11] scene_away.png OK (671 bytes)
I (8694) UI_ASSET:   cache[12] scene_sleep.png OK (772 bytes)
I (8699) UI_ASSET:   cache[13] scene_movie.png OK (652 bytes)
I (8704) UI_ASSET:   cache[14] scene_night.png OK (831 bytes)
I (8710) UI_ASSET:   cache[15] scene_rain.png OK (1485 bytes)
I (8715) UI_ASSET:   cache[16] scene_fire.png OK (2523 bytes)
I (8721) UI_ASSET:   cache[17] color_wheel_220.png OK (20412 bytes)
I (8727) UI_ASSET: === Diagnosis Complete ===
I (8731) ALARM_UI: Fire alarm UI initialized with 500ms polling
I (8737) UI_MGR: UI initialized in embedded content mode, 7 pages
I (8746) UI_ASSET: asset OK(cache): siyin_logo.png (9857 bytes)
I (8760) UI_ASSET: asset image pre-decoded: siyin_logo.png size=330x60 bytes=79200
I (8765) UI_ASSET: asset OK(cache): scene_lights.png (786 bytes)
I (8767) UI_ASSET: asset image pre-decoded: scene_lights.png size=34x34 bytes=4624
I (8770) UI_ASSET: asset OK(cache): scene_home.png (724 bytes)
I (8776) UI_ASSET: asset image pre-decoded: scene_home.png size=34x34 bytes=4624
I (8783) UI_ASSET: asset OK(cache): scene_away.png (671 bytes)
I (8788) UI_ASSET: asset image pre-decoded: scene_away.png size=34x34 bytes=4624
I (8796) UI_ASSET: asset OK(cache): scene_sleep.png (772 bytes)
I (8801) UI_ASSET: asset image pre-decoded: scene_sleep.png size=34x34 bytes=4624
I (8809) UI_ASSET: asset OK(cache): scene_movie.png (652 bytes)
I (8814) UI_ASSET: asset image pre-decoded: scene_movie.png size=34x34 bytes=4624
I (8822) UI_ASSET: asset OK(cache): scene_night.png (831 bytes)
I (8827) UI_ASSET: asset image pre-decoded: scene_night.png size=34x34 bytes=4624
I (8839) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (8844) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (8849) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (8856) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (8871) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (8874) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (8876) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (8880) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (8887) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (8892) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
I (8898) PAGE_DATA: Data overview page created
I (9071) LcdDisplay: Shell page switch: overview
I (9077) UI_ASSET: asset OK(cache): siyin_logo.png (9857 bytes)
I (9089) UI_ASSET: asset image pre-decoded: siyin_logo.png size=330x60 bytes=79200
I (9094) UI_ASSET: asset OK(cache): scene_lights.png (786 bytes)
I (9096) UI_ASSET: asset image pre-decoded: scene_lights.png size=34x34 bytes=4624
I (9100) UI_ASSET: asset OK(cache): scene_home.png (724 bytes)
I (9105) UI_ASSET: asset image pre-decoded: scene_home.png size=34x34 bytes=4624
I (9112) UI_ASSET: asset OK(cache): scene_away.png (671 bytes)
I (9118) UI_ASSET: asset image pre-decoded: scene_away.png size=34x34 bytes=4624
I (9125) UI_ASSET: asset OK(cache): scene_sleep.png (772 bytes)
I (9131) UI_ASSET: asset image pre-decoded: scene_sleep.png size=34x34 bytes=4624
I (9138) UI_ASSET: asset OK(cache): scene_movie.png (652 bytes)
I (9143) UI_ASSET: asset image pre-decoded: scene_movie.png size=34x34 bytes=4624
I (9151) UI_ASSET: asset OK(cache): scene_night.png (831 bytes)
I (9156) UI_ASSET: asset image pre-decoded: scene_night.png size=34x34 bytes=4624
I (9168) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (9173) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (9178) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (9185) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (9200) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (9203) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (9205) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (9209) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (9216) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (9222) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
I (9228) PAGE_DATA: Data overview page created
I (9595) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (9600) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (9604) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (9607) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (9624) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (9626) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (9629) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (9633) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (9640) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (9645) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
I (10580) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (10586) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (10589) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (10592) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (10609) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (10612) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (10615) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (10618) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (10626) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (10631) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
I (11347) SystemInfo: free sram: 80067 minimal sram: 68459
I (11565) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (11570) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (11573) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (11577) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (11594) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (11597) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (11599) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (11603) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (11611) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (11616) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
I (12555) UI_ASSET: asset OK(cache): overview_home.png (1838 bytes)
I (12561) UI_ASSET: asset image pre-decoded: overview_home.png size=148x82 bytes=48544
I (12564) UI_ASSET: asset OK(cache): weather_guangzhou.png (1381 bytes)
I (12567) UI_ASSET: asset image pre-decoded: weather_guangzhou.png size=88x70 bytes=24640
I (12585) UI_ASSET: asset OK(cache): floor_1.png (981 bytes)
I (12587) UI_ASSET: asset image pre-decoded: floor_1.png size=82x56 bytes=18368
I (12590) UI_ASSET: asset OK(cache): floor_2.png (999 bytes)
I (12594) UI_ASSET: asset image pre-decoded: floor_2.png size=82x56 bytes=18368
I (12601) UI_ASSET: asset OK(cache): floor_3.png (1005 bytes)
I (12606) UI_ASSET: asset image pre-decoded: floor_3.png size=82x56 bytes=18368
E BOD: Brownout detector was triggered