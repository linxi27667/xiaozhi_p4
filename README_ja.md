SPI mode:DIO, clock div:2
load:0x4ff33ce0,len:0x5a0
load:0x4ff29ed0,len:0xd14
load:0x4ff2cbd0,len:0x32e8
entry 0x4ff29ed0
I (46) hex_psram: vendor id    : 0x0d (AP)
I (46) hex_psram: Latency      : 0x01 (Fixed)
I (47) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (47) hex_psram: dev id       : 0x03 (generation 4)
I (52) hex_psram: density      : 0x07 (256 Mbit)
I (56) hex_psram: good-die     : 0x06 (Pass)
I (60) hex_psram: SRF          : 0x02 (Slow Refresh)
I (65) hex_psram: BurstType    : 0x00 ( Wrap)
I (69) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (73) hex_psram: BitMode      : 0x01 (X16 Mode)
I (77) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
I (82) hex_psram: DriveStrength: 0x00 (1/1)
I (86) MSPI Timing: Enter psram timing tuning
I esp_psram: Found 32MB PSRAM device
I esp_psram: Speed: 200MHz
I (510) mmu_psram: .rodata xip on psram
I (795) mmu_psram: .text xip on psram
I (796) hex_psram: psram CS IO is dedicated
I (796) cpu_start: Multicore app
I (805) cpu_start: GPIO 38 and 37 are used as console UART I/O p
I (806) cpu_start: Pro cpu start user code
I (806) cpu_start: cpu freq: 360000000 Hz
I (808) app_init: Application information:
I (811) app_init: Project name:     xiaozhi
I (815) app_init: App version:      2.2.4
I (819) app_init: Compile time:     Jun 23 2026 11:01:42
I (824) app_init: ELF file SHA256:  fc0a6ac37...
I (828) app_init: ESP-IDF:          v5.5.3-dirty
I (833) efuse_init: Min chip rev:     v0.0
I (837) efuse_init: Max chip rev:     v1.99
I (841) efuse_init: Chip rev:         v1.3
I (844) heap_init: Initializing. RAM available for dynamic alloc
I (851) heap_init: At 4FF26F10 len 000140B0 (80 KiB): RETENT_RAM
I (856) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
I (861) heap_init: At 4FF40000 len 00060000 (384 KiB): RAM
I (867) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
I (872) esp_psram: Adding pool of 26624K of PSRAM memory to heap
I (878) esp_psram: Adding pool of 12K of PSRAM memory gap genera
ess alignment of irom to the heap allocator
I (889) esp_psram: Adding pool of 5K of PSRAM memory gap generat
ss alignment of drom to the heap allocator
I (901) spi_flash: detected chip: gd
I (904) spi_flash: flash io: qio
I (946) sleep_gpio: Configure to isolate all GPIO pins in sleep 
I (953) sleep_gpio: Enable automatic switching of GPIO sleep con
I (959) main_task: Started on CPU0
I (962) esp_psram: Reserving pool of 64K of internal memory for 
tions
I (970) main_task: Calling app_main()
I (981) Board: UUID=bfba9048-2a61-4c2a-a7b0-1de80e6c0f4d SKU=esp
rd
I (981) button: IoT Button Version: 4.1.6
W (984) i2c.master: Please check pull-up resistances whether be 
 Otherwise unexpected behavior would happen. For more detailed i
read docs
I (1000) ESP32_P4_EV: MIPI DSI PHY Powered on
I (1005) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (1009) ESP32_P4_EV: Install EK79007 LCD control panel
I (1014) ek79007: version: 1.0.4
I (1181) ESP32_P4_EV: Display initialized
I (1181) Display: Power management not supported
I (1181) LcdDisplay: Initialize LVGL library
I (1182) LcdDisplay: Initialize LVGL port
I (1185) LVGL: Starting LVGL task
I (1188) LcdDisplay: Adding LCD display
I (1199) GT911: I2C address initialization procedure skipped - u
setup
I (1200) GT911: TouchPad_ID:0x39,0x31,0x31
I (1204) GT911: TouchPad_Config_Version:89
I (1207) ESP32P4FuncEV: Touch controller registered with LVGL
I (1213) ESP32P4_TF: Initializing TF card
I (1267) ESP32P4_TF: SD card power enabled: GPIO45(SD_PWRn)=0, L
I (1267) ESP32P4_TF: Mounting TF card via SDMMC slot 0, 1-bit, 2
=44 D0=39
I (1271) sdmmc_periph: sdmmc_host_init: SDMMC host already initi
it flow
I (1322) ESP32P4_TF: TF card mounted successfully at /sdcard (SD
Name: SD
Type: SDHC
Speed: 20.00 MHz (limit: 20.00 MHz)
Size: luMB
CSD: ver=2, sector_size=512, capacity=15613952 read_bl_len=9
SSR: bus_width=1
I (1333) ESP32P4_TF: UI asset folder found: /sdcard/xiaozhi_ui
I (1337) UI_ASSET: Preloading TF UI assets from /sdcard/xiaozhi_
O runtime traffic
I (1585) UI_ASSET: TF asset preload complete: 18/18 files, 50127
I (1585) ESP32P4FuncEV: Initializing camera (MIPI-CSI)
E (1585) EspVideo: open /dev/video0 failed, errno=2(No such file
I (1592) ESP32P4FuncEV: MIPI-CSI camera initialized successfully
I (1598) ESP32P4FuncEV: Initializing font support
I (1602) ESP32P4FuncEV: Custom font loaded successfully: line_he
W (1609) ledc: GPIO 26 is not usable, maybe conflict with others
I (1614) Backlight: Set brightness to 75
I (1618) StateMachine: State: unknown -> starting
I (1671) Es8311AudioCodec: Duplex channels created
I (1676) ES8311: Work in Slave mode
I (1679) Es8311AudioCodec: Es8311AudioCodec initialized
I (1679) AudioCodec: Audio codec started
I (1681) MCP: Add tool: self.get_device_status
I (1681) MCP: Add tool: self.audio_speaker.set_volume
I (1686) MCP: Add tool: self.screen.set_brightness
I (1690) MCP: Add tool: self.screen.set_theme
I (1694) MCP: Add tool: self.camera.take_photo
I (1698) MCP: Add tool: self.get_system_info [user]
I (1703) MCP: Add tool: self.reboot [user]
I (1707) MCP: Add tool: self.upgrade_firmware [user]
I (1711) MCP: Add tool: self.screen.get_info [user]
I (1716) MCP: Add tool: self.screen.snapshot [user]
I (1721) MCP: Add tool: self.screen.preview_image [user]
I (1726) Assets: The storage free size is 65536 KB
I (1730) Assets: The partition size is 3072 KB
I (1848) Assets: The checksum calculation time is 113 ms
I (1848) MCP: Add tool: self.assets.set_download_url [user]
I (1848) MCP: Add tool: self.iot.get_status
I (1851) MCP: Add tool: self.iot.get_sensors
I (1855) MCP: Add tool: self.iot.discover
I (1859) MCP: Add tool: self.iot.set_gpio
I (1862) MCP: Add tool: self.iot.set_light
I (1866) MCP: Add tool: self.iot.set_rgb_light
I (1870) MCP: Add tool: self.iot.set_scene
I (1874) MCP: Add tool: self.iot.set_relay
I (1878) MCP: Add tool: self.iot.set_servo_by_index
I (1882) MCP: Add tool: self.iot.all_off
I (1886) MCP: Add tool: self.iot.all_on
I (1890) MCP: Add tool: self.iot.all_lights_off
I (1894) MCP: Add tool: self.iot.all_lights_on
I (1898) MCP: Add tool: self.face.list
I (1902) MCP: Add tool: self.face.delete
I (1905) MCP: Add tool: self.face.clear_all
I (1909) MCP: Add tool: self.face.recognize
I (1913) MCP: Add tool: self.face.capture
I (1917) MCP: Add tool: self.face.register
I (1921) FaceMcp: Face MCP tools registered
I (1925) DEV_MODEL: Model initialized: 11 devices
I (1929) EVT_CENTER: Event center initialized
I (1933) AUTO_MODE: Auto mode initialized, global=1
I (1938) RULE: Rule engine initialized
I (1941) SH_MQTT: Smart-home MQTT initialized
I (1945) SmartHomeTasks: Initializing SNTP

assert failed: tcpip_callback /IDF/components/lwip/lwip/src/api/
d mbox)
Core  0 register dump:
MEPC    : 0x4ff0bc70  RA      : 0x4ff0b810  SP      : 0x4ff361f0
780
..--- 0x4ff0bc70: panic_abort at E:/MCU/esp32/.espressif/v5.5.3/
esp_system/panic.c:491
--- 0x4ff0b810: esp_vApplicationTickHook at E:/MCU/esp32/.espres
components/esp_system/freertos_hooks.c:31
TP      : 0x4ff36410  T0      : 0x37363534  T1      : 0x7271706f