I (43) hex_psram: vendor id    : 0x0d (AP)
I (43) hex_psram: Latency      : 0x01 (Fixed)
I (44) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (44) hex_psram: dev id       : 0x03 (generation 4)
I (49) hex_psram: density      : 0x07 (256 Mbit)
I (53) hex_psram: good-die     : 0x06 (Pass)
I (57) hex_psram: SRF          : 0x02 (Slow Refresh)
I (62) hex_psram: BurstType    : 0x00 ( Wrap)
I (66) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (70) hex_psram: BitMode      : 0x01 (X16 Mode)
I (74) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
I (79) hex_psram: DriveStrength: 0x00 (1/1)
I (83) MSPI Timing: Enter psram timing tuning
I esp_psram: Found 32MB PSRAM device
I esp_psram: Speed: 200MHz
I (334) mmu_psram: .rodata xip on psram
I (471) mmu_psram: .text xip on psram
I (471) hex_psram: psram CS IO is dedicated
I (472) cpu_start: Multicore app
I (481) cpu_start: GPIO 38 and 37 are used as console UART I/O pins
I (482) cpu_start: Pro cpu start user code
I (482) cpu_start: cpu freq: 360000000 Hz
I (484) app_init: Application information:
I (487) app_init: Project name:     xiaozhi
I (491) app_init: App version:      2.2.4
I (495) app_init: Compile time:     Jun 20 2026 17:10:09
I (500) app_init: ELF file SHA256:  799ef3edc...
I (504) app_init: ESP-IDF:          v5.5.3-dirty
I (509) efuse_init: Min chip rev:     v1.0
I (513) efuse_init: Max chip rev:     v1.99
I (516) efuse_init: Chip rev:         v1.3
I (520) heap_init: Initializing. RAM available for dynamic allocation:
I (527) heap_init: At 4FF28030 len 00012F90 (75 KiB): RETENT_RAM
I (532) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
I (537) heap_init: At 4FF40000 len 00060000 (384 KiB): RAM
I (543) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
I (548) esp_psram: Adding pool of 29056K of PSRAM memory to heap allocator
I (554) esp_psram: Adding pool of 3K of PSRAM memory gap generated due to end address alignment of irom to the heap allocator
I (565) esp_psram: Adding pool of 60K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
I (577) spi_flash: detected chip: gd
I (580) spi_flash: flash io: qio
I (622) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (628) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (635) main_task: Started on CPU0
I (638) esp_psram: Reserving pool of 64K of internal memory for DMA/internal allocations
I (646) main_task: Calling app_main()
I (656) Board: UUID=bfba9048-2a61-4c2a-a7b0-1de80e6c0f4d SKU=esp-p4-function-ev-board
I (657) button: IoT Button Version: 4.1.6
W (660) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
I (676) ESP32_P4_EV: MIPI DSI PHY Powered on
I (681) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (685) ESP32_P4_EV: Install EK79007 LCD control panel
I (690) ek79007: version: 1.0.4
I (857) ESP32_P4_EV: Display initialized
I (857) Display: Power management not supported
I (857) LcdDisplay: Initialize LVGL library
I (857) LcdDisplay: Initialize LVGL port
I (861) LVGL: Starting LVGL task
I (864) LcdDisplay: Adding LCD display
I (868) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (876) GT911: TouchPad_ID:0x39,0x31,0x31
I (879) GT911: TouchPad_Config_Version:89
I (883) ESP32P4FuncEV: Touch controller registered with LVGL
I (888) ESP32P4FuncEV: Initializing TF card
W (892) ldo: The voltage value 0 is out of the recommended range [500, 2700]        
I (899) ESP32P4FuncEV: SD LDO_VO4 prepared at 3300mV
I (1723) ESP32P4FuncEV: TF power cycled: GPIO45(SD_PWRn)=0, LDO_VO4=3300mV
I (1723) ESP32P4FuncEV: Mounting TF card via SDMMC slot 0, 1-bit, 400kHz: CLK=43 CMD=44 D0=39
E (1754) sdmmc_common: sdmmc_init_ocr: send_op_cond (1) returned 0x107
E (1754) vfs_fat_sdmmc: sdmmc_card_init failed (0x107).
W (1754) ESP32P4FuncEV: SDMMC TF mount failed: ESP_ERR_TIMEOUT; retrying via SDSPI fallback
I (2582) ESP32P4FuncEV: TF power cycled: GPIO45(SD_PWRn)=0, LDO_VO4=3300mV
I (2582) ESP32P4FuncEV: Mounting TF card via SDSPI, 400kHz: SCLK=43 MOSI=44 MISO=39 CS=42
E (2607) vfs_fat_sdmmc: sdmmc_card_init failed (0x107).
E (2608) ESP32P4FuncEV: TF card initialization failed: ESP_ERR_TIMEOUT
W (2608) UI_ASSET: /sdcard is not mounted; board initialization owns TF card mounting
E (2615) UI_ASSET: TF asset preload skipped: /sdcard is not mounted
I (2621) ESP32P4FuncEV: Initializing camera
I (2625) ESP32P4FuncEV: Camera initialized successfully via BSP
I (2631) ESP32P4FuncEV: Initializing font support
I (2635) ESP32P4FuncEV: Custom font loaded successfully: line_height=25
W (2641) ledc: GPIO 26 is not usable, maybe conflict with others
I (2647) Backlight: Set brightness to 75
I (2651) StateMachine: State: unknown -> starting
I (2698) Es8311AudioCodec: Duplex channels created
I (2703) ES8311: Work in Slave mode
I (2706) Es8311AudioCodec: Es8311AudioCodec initialized
I (2706) AudioCodec: Audio codec started
I (2708) MCP: Add tool: self.get_device_status
I (2709) MCP: Add tool: self.audio_speaker.set_volume
I (2713) MCP: Add tool: self.screen.set_brightness
I (2718) MCP: Add tool: self.screen.set_theme
I (2722) MCP: Add tool: self.get_system_info [user]
I (2726) MCP: Add tool: self.reboot [user]
I (2730) MCP: Add tool: self.upgrade_firmware [user]
I (2735) MCP: Add tool: self.screen.get_info [user]
I (2739) MCP: Add tool: self.screen.snapshot [user]
I (2744) MCP: Add tool: self.screen.preview_image [user]
I (2749) Assets: The storage free size is 65536 KB
I (2753) Assets: The partition size is 8192 KB
I (2847) Assets: The checksum calculation time is 89 ms
I (2847) MCP: Add tool: self.assets.set_download_url [user]
I (2847) MCP: Add tool: self.iot.get_status
I (2850) MCP: Add tool: self.iot.get_sensors
I (2854) MCP: Add tool: self.iot.discover
I (2858) MCP: Add tool: self.iot.set_gpio
I (2861) MCP: Add tool: self.iot.set_light
I (2865) MCP: Add tool: self.iot.set_rgb_light
I (2869) MCP: Add tool: self.iot.set_scene
I (2873) MCP: Add tool: self.iot.set_relay
I (2877) MCP: Add tool: self.iot.set_servo_by_index
I (2882) MCP: Add tool: self.iot.all_off
I (2885) MCP: Add tool: self.iot.all_on
I (2889) MCP: Add tool: self.iot.all_lights_off
I (2893) MCP: Add tool: self.iot.all_lights_on
I (2897) DEV_MODEL: Model initialized: 11 devices
I (2902) SH_MQTT: Smart-home MQTT initialized
I (2906) SmartHomeTasks: Smart home task layer started
I (2912) Application: Network start task started
I (2915) WifiManager: Initializing...
I (2919) transport: Attempt connection with slave: retry[0]
I (2924) transport: Reset slave using GPIO[54]
I (2928) os_wrapper_esp: GPIO [54] configured
I (4535) sdio_wrapper: SDIO master: Slot 1, Data-Lines: 4-bit Freq(KHz)[40000 KHz]
I (4535) sdio_wrapper: GPIOs: CLK[18] CMD[19] D0[14] D1[15] D2[16] D3[17] Slave_Reset[54]
I (4535) H_SDIO_DRV: Starting SDIO process rx task
I (4539) sdio_wrapper: Queues: Tx[20] Rx[20] SDIO-Rx-Mode[1]

assert failed: xQueueSemaphoreTake queue.c:1709 (( pxQueue ))
Core  0 register dump:
MEPC    : 0x4ff0d6d0  RA      : 0x4ff0d270  SP      : 0x4ff2d380  GP      : 0x4ff1ab00
--- 0x4ff0d6d0: panic_abort at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_system/panic.c:491
--- 0x4ff0d270: esp_vApplicationTickHook at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_system/freertos_hooks.c:31
TP      : 0x4ff2d810  T0      : 0x37363534  T1      : 0x7271706f  T2      : 0x33323130
S0/FP   : 0x00000001  S1      : 0x0000008a  A0      : 0x4ff2d3c8  A1      : 0x4ff1fc29
A2      : 0x00000001  A3      : 0x4ff27000  A4      : 0x00000001  A5      : 0x4ff27000
A6      : 0x0000000c  A7      : 0x76757473  S2      : 0x4ff2d3c8  S3      : 0x4ff2d3cd
S4      : 0x4ff1fc28  S5      : 0x4ff2d3c8  S6      : 0x00000000  S7      : 0x00000001
S8      : 0x00000000  S9      : 0x4ff28000  S10     : 0x00000000  S11     : 0x00000000
T3      : 0x6e6d6c6b  T4      : 0x6a696867  T5      : 0x66656463  T6      : 0x62613938
MSTATUS : 0x00011880  MTVEC   : 0x4ff00003  MCAUSE  : 0x00000002  MTVAL   : 0x00000000
--- 0x4ff00003: _vector_table at ??:?
MHARTID : 0x00000000

Stack memory:
4ff2d380: 0x4ff38f98 0xffffffff 0x4828f6c0 0x4ff17a4c 0x39303731 0x4ff00300 0x00000000 0x00000000
--- 0x4ff17a4c: esp_ptr_in_drom at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_hw_support/include/esp_memory_utils.h:337
--- (inlined by) __assert_func at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/newlib/src/assert.c:62
--- 0x4ff00300: _interrupt_handler at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/riscv/vectors.S:511
4ff2d3a0: 0x4f000000 0x4ff206ec 0x4828f6c0 0x4ff206d0 0x48253d7e 0x4ff206fc 0x4ff2d390 0x4ff20700
4ff2d3c0: 0x48253fdc 0x4ff1fc28 0x65737361 0x66207472 0x656c6961 0x78203a64 0x75657551 0x6d655365
4ff2d3e0: 0x6f687061 0x61546572 0x7120656b 0x65756575 0x313a632e 0x20393037 0x70202828 0x65755178
4ff2d400: 0x29206575 0x00000029 0x00000000 0x00000000 0x00000000 0x4ff2d86c 0xffffffff 0x00000000
4ff2d420: 0x4ff29a90 0x4ff2d86c 0x4ff38fa4 0x4ff0e3b4 0x4ff1a980 0x4ff27760 0x4ff29acd 0x48241f96
--- 0x4ff0e3b4: xQueueGiveMutexRecursive at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/freertos/FreeRTOS-Kernel/queue.c:796
--- 0x48241f96: usb_serial_jtag_write at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c:206
--- (inlined by) usb_serial_jtag_write at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c:183
4ff2d440: 0x00000000 0x00000002 0x00000001 0x00000025 0x0000000a 0xffffffff 0xffffffff 0x4ff29a90
4ff2d460: 0x4ff29a90 0xffffffff 0x00000000 0x00000000 0x00000000 0x00000001 0x4ff2d86c 0x4ff12240
--- 0x4ff12240: xTaskPriorityDisinherit at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/freertos/FreeRTOS-Kernel/tasks.c:5221
4ff2d480: 0x4ff2ede4 0x00000000 0x4ff28724 0x4fc0aa06 0x00000000 0x00000000 0x00000000 0x48299000
--- 0x4fc0aa06: UartSecureDwnLdProc in ROM
4ff2d4a0: 0x4ff2d564 0x4ff2ad34 0x48283000 0x4ff0e8c6 0x0000000a 0x4ff2d8d4 0x4ff28724 0xffffffff
--- 0x4ff0e8c6: xQueueSemaphoreTake at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/freertos/FreeRTOS-Kernel/queue.c:1713
4ff2d4c0: 0x4ff28724 0x4ff2d8d4 0x4ff2d510 0x4ff28724 0x4ff2d8d4 0x4825db77 0x00000000 0x48299000
4ff2d4e0: 0x4ff2d564 0x4ff2ad34 0x48283000 0x481c5cae 0x4825db75 0x4ff28724 0x4825db77 0x4823c80c
--- 0x481c5cae: handle_idle_state_events at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_driver_sdmmc/src/sdmmc_transaction.c:206
--- (inlined by) sdmmc_host_do_transaction at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/esp_driver_sdmmc/src/sdmmc_transaction.c:116
--- 0x4823c80c: _vfprintf_r at /builds/idf/crosstool-NG/.build/riscv32-esp-elf/src/newlib/newlib/libc/stdio/nano-vfprintf.c:650
4ff2d500: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
4ff2d520: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x48299000
4ff2d540: 0x00000000 0x4ff2ad34 0x4ff2d564 0x481c05d4 0x00000003 0x4ff27000 0x00000000 0x481c2e6a
--- 0x481c05d4: sdmmc_send_cmd at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/sdmmc/sdmmc_cmd.c:25
--- 0x481c2e6a: sdmmc_io_reset at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/sdmmc/sdmmc_io.c:69
4ff2d560: 0x4ff27000 0x00000034 0x80000c08 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
4ff2d580: 0x00000000 0x00000000 0x00000000 0x00001c00 0x00000000 0x000003e8 0x00000000 0x00000000
4ff2d5a0: 0xa5a5a5a5 0x4ff2ad34 0x00000000 0x481c288a 0xa5a5a5a5 0x00000000 0x4ff2300c 0x48018fa6
--- 0x481c288a: sdmmc_card_init at E:/MCU/esp32/.espressif/v5.5.3/esp-idf/components/sdmmc/sdmmc_init.c:75
--- 0x48018fa6: hosted_sdio_card_init at E:/MCU/esp32/p4/xiaozhi-for-p4/managed_components/espressif__esp_hosted/host/port/esp/freertos/src/sdio_wrapper.c:404
4ff2d5c0: 0xa5a5a5a5 0xa5a5a5a5 0x4ff2300c 0x000011bb 0x4ff27000 0x00000000 0x4ff2300c 0x48018fba
--- 0x48018fba: hosted_sdio_card_init at E:/MCU/esp32/p4/xiaozhi-for-p4/managed_components/espressif__esp_hosted/host/port/esp/freertos/src/sdio_wrapper.c:407
4ff2d5e0: 0x0000000f 0x00000010 0x00000011 0x00000036 0xa5a5a5a5 0xa5a5a5a5 0xa5a5a5a5 0x00000077
4ff2d600: 0x00000001 0x00009c40 0x40533333 0x00000000 0x00000000 0x481c772c 0x481c8456 0x481c85be