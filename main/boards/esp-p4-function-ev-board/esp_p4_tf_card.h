#pragma once

#include <esp_err.h>
#include <esp_ldo_regulator.h>
#include <sdmmc_cmd.h>

// Follows the BSP official driver path (esp_ldo_regulator + SDMMC_FREQ_DEFAULT).
// The previous implementation used sd_pwr_ctrl_by_on_chip_ldo which is designed
// for SD3.0 1.8V voltage switching (range 500-2700mV), not for the 3.3V main
// power required by the TF card. This caused ESP_ERR_TIMEOUT at send_op_cond.
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
    esp_ldo_channel_handle_t ldo_chan_ = nullptr;
    bool mounted_ = false;

    esp_err_t EnablePower();
    void ReleasePower();
    void LogRootEntries() const;
    void VerifyAssetFolder() const;
};
