#include "ui_device_settings.h"

#include "audio_codec.h"
#include "board.h"

namespace {

int ClampPercent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

int ClampBrightness(int value)
{
    value = ClampPercent(value);
    return value < 10 ? 10 : value;
}

}  // namespace

extern "C" int ui_device_settings_get_brightness(void)
{
    auto backlight = Board::GetInstance().GetBacklight();
    return backlight ? ClampBrightness(backlight->brightness()) : 80;
}

extern "C" void ui_device_settings_set_brightness(int brightness, bool permanent)
{
    auto backlight = Board::GetInstance().GetBacklight();
    if (backlight) {
        backlight->SetBrightness(static_cast<uint8_t>(ClampBrightness(brightness)), permanent);
    }
}

extern "C" int ui_device_settings_get_volume(void)
{
    auto codec = Board::GetInstance().GetAudioCodec();
    return codec ? ClampPercent(codec->output_volume()) : 70;
}

extern "C" void ui_device_settings_set_volume(int volume)
{
    auto codec = Board::GetInstance().GetAudioCodec();
    volume = ClampPercent(volume);
    if (codec && codec->output_volume() != volume) {
        codec->SetOutputVolume(volume);
    }
}
