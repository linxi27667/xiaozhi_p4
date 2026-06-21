#pragma once

#include <esp_err.h>
#include <sdmmc_cmd.h>

#include "sd_pwr_ctrl.h"

class EspP4TfCard {
public:
    EspP4TfCard() = default;
    ~EspP4TfCard();

    esp_err_t Mount();
    void Unmount();

    bool IsMounted() const { return mounted_; }
    sdmmc_card_t *card() const { return card_; }

private:
    sdmmc_card_t *card_ = nullptr;
    sd_pwr_ctrl_handle_t pwr_ctrl_ = nullptr;
    bool mounted_ = false;

    esp_err_t EnsurePowerController();
    esp_err_t PowerCycleCard();
    esp_err_t MountSdmmc();
    void PrepareSdPins();
    void ReleasePower();
    void LogRootEntries() const;
    void VerifyAssetFolder() const;
};
