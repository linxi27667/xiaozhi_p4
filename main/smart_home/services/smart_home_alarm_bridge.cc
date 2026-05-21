#include "application.h"
#include "assets/lang_config.h"
#include "smart_home_alarm_ui.h"

#include <atomic>
#include <cstdio>

static std::atomic<bool> s_fire_active{false};

extern "C" void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active)
{
    smart_home_alarm_ui_set_fire(floor_id, active);

    bool was_active = s_fire_active.exchange(active);
    if (!active || was_active) {
        return;
    }

    Application::GetInstance().Schedule([floor_id]() {
        char message[128];
        const char *floor = floor_id == 1 ? "一楼" : floor_id == 2 ? "二楼" : floor_id == 3 ? "三楼" : "未知楼层";
        snprintf(message, sizeof(message), "检测到%s发生火灾，请立即检查现场设备。", floor);
        auto& app = Application::GetInstance();
        app.Alert("火灾警报", message, "triangle_exclamation", Lang::Sounds::OGG_EXCLAMATION);
        app.WakeWordInvoke("fire_alarm");
    });
}
