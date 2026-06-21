$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$boardPath = Join-Path $repoRoot 'main/boards/esp-p4-function-ev-board/esp-p4-function-ev-board.cc'
$tfDriverPath = Join-Path $repoRoot 'main/boards/esp-p4-function-ev-board/esp_p4_tf_card.cc'
$tfDriverHeaderPath = Join-Path $repoRoot 'main/boards/esp-p4-function-ev-board/esp_p4_tf_card.h'
$uiPath = Join-Path $repoRoot 'main/smart_home/ui/services/ui_asset_service.c'
$cmakePath = Join-Path $repoRoot 'main/CMakeLists.txt'
$sdkconfigPath = Join-Path $repoRoot 'sdkconfig'

$board = Get-Content -Raw -LiteralPath $boardPath
$tfDriver = if (Test-Path -LiteralPath $tfDriverPath) { Get-Content -Raw -LiteralPath $tfDriverPath } else { '' }
$tfDriverHeader = if (Test-Path -LiteralPath $tfDriverHeaderPath) { Get-Content -Raw -LiteralPath $tfDriverHeaderPath } else { '' }
$ui = Get-Content -Raw -LiteralPath $uiPath
$cmake = Get-Content -Raw -LiteralPath $cmakePath
$sdkconfig = Get-Content -Raw -LiteralPath $sdkconfigPath

$failures = New-Object System.Collections.Generic.List[string]
function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        $script:failures.Add($message)
    }
}

Require (Test-Path -LiteralPath $tfDriverPath) 'TF card code must live in esp_p4_tf_card.cc, not inside the board constructor file.'
Require (Test-Path -LiteralPath $tfDriverHeaderPath) 'TF card driver must expose a small esp_p4_tf_card.h interface.'

Require ($board -match '#include "esp_p4_tf_card.h"') 'Board must depend on the small TF driver interface.'
Require ($board -match 'EspP4TfCard\s+tf_card_') 'Board must own one TF driver instance.'
Require ($board -notmatch '\bbsp_sdcard_mount\s*\(') 'Board TF init must not use bsp_sdcard_mount(); it hides power-control and retry policy.'
Require ($board -notmatch 'esp_vfs_fat_sdmmc_mount|esp_vfs_fat_sdspi_mount|sd_pwr_ctrl_|SDSPI_HOST_DEFAULT|sdmmc_host_deinit_slot|spi_bus_free') 'Board file must not contain low-level TF/SDMMC/SDSPI plumbing.'

Require ($tfDriver -match 'sd_pwr_ctrl_new_on_chip_ldo') 'TF driver must create the ESP32-P4 on-chip SD LDO controller.'
Require ($tfDriver -match 'pwr_ctrl_handle\s*=') 'TF driver must pass host.pwr_ctrl_handle to SDMMC.'
Require ($tfDriver -match 'sd_pwr_ctrl_set_io_voltage') 'TF driver must pre-bias LDO_VO4 to 3.3V before releasing SD_PWRn.'
Require ($tfDriver -match 'SDMMC_FREQ_PROBING') 'TF driver must bring the card up at 400 kHz for robust probing.'
Require ($tfDriver -match 'SDMMC_HOST_FLAG_1BIT') 'TF driver must force 1-bit SDMMC mode.'
Require ($tfDriver -match 'slot\.width\s*=\s*1') 'TF driver must configure the slot as 1-bit.'
Require ($tfDriver -match 'SDMMC_SLOT_FLAG_INTERNAL_PULLUP') 'TF driver must enable SD line pull-ups during bring-up.'
Require ($tfDriver -match 'esp_vfs_fat_sdmmc_mount') 'TF driver must mount through the SDMMC VFS helper.'
Require ($tfDriver -notmatch 'SDSPI_HOST_DEFAULT|esp_vfs_fat_sdspi_mount|spi_bus_initialize|spi_bus_free') 'TF driver must not use SDSPI fallback in the boot-critical path.'
Require ($tfDriver -notmatch 'sdmmc_host_deinit_slot') 'TF driver must not manually deinit SDMMC slots; VFS mount/unmount owns host cleanup.'
Require ($tfDriver -match 'gpio_set_level\s*\(\s*kSdPowerEnableGpio\s*,\s*1\s*\)') 'TF driver must fully disable SD_PWRn before probing.'
Require ($tfDriver -match 'gpio_set_level\s*\(\s*kSdPowerEnableGpio\s*,\s*0\s*\)') 'TF driver must enable SD_PWRn before probing.'
Require ($tfDriverHeader -match 'class\s+EspP4TfCard') 'TF driver header must expose EspP4TfCard.'

Require ($ui -notmatch 'asset NOT preloaded') 'UI asset service must not print asset NOT preloaded on every refresh.'
Require ($ui -notmatch 'image_create SKIP \(no asset\)') 'UI asset service must not print image_create SKIP on every refresh.'
Require ($ui -match 's_sdcard_missing_logged') 'UI asset service must gate repeated /sdcard missing warnings.'
Require ($ui -match 's_assets_unavailable') 'UI asset service must permanently degrade when required TF assets are unavailable.'

Require ($cmake -match '(?m)^\s*sdmmc\s*$') 'main/CMakeLists.txt must explicitly require sdmmc.'
Require ($cmake -match '(?m)^\s*esp_driver_sdmmc\s*$') 'main/CMakeLists.txt must explicitly require esp_driver_sdmmc.'
Require ($sdkconfig -match 'CONFIG_FATFS_LFN_HEAP=y') 'sdkconfig should keep FATFS long filename support enabled.'

if ($failures.Count -gt 0) {
    Write-Host 'TF card chain check FAILED:'
    foreach ($failure in $failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host 'TF card chain check PASSED'
