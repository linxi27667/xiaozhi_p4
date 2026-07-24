#include "gesture_mcp_tool.h"

#include "mcp_server.h"
#include "smart_home/services/gesture_mode.h"

#include <cJSON.h>

namespace {

cJSON *BuildStatus() {
    gesture_mode_snapshot_t snapshot = {};
    gesture_mode_get_snapshot(&snapshot);

    cJSON *result = cJSON_CreateObject();
    const bool ready =
        snapshot.active && snapshot.state == GESTURE_STATE_RUNNING;
    cJSON_AddBoolToObject(result, "active", snapshot.active);
    cJSON_AddBoolToObject(result, "ready", ready);
    cJSON_AddBoolToObject(result, "requested", snapshot.active);
    cJSON_AddStringToObject(result, "state", gesture_mode_state_name(snapshot.state));
    cJSON_AddNumberToObject(result, "timeout_seconds", snapshot.timeout_seconds);
    cJSON_AddStringToObject(result, "status", snapshot.status);
    if (snapshot.error[0] != '\0') {
        cJSON_AddStringToObject(result, "reason", snapshot.error);
    }
    return result;
}

}  // namespace

extern "C" void GestureMcp_RegisterTools(void) {
    auto &server = McpServer::GetInstance();
    server.AddTool(
        "self.gesture.set_mode",
        "开启或退出手势控制模式、隔空控制模式。enabled=true 后请先完成本轮语音提示；"
        "设备随后关闭在线语音并进入独占手势界面，只能通过右上角触屏退出恢复小鑫。"
        "该模式支持握拳=离家、全开手掌=回家、OK=观影、数字2=开门、数字3=关门、"
        "数字4=明亮；握拳属于实验识别，需要保持动作直至界面显示4/4。只有返回 "
        "active=true 且 ready=true、state=running 才表示已可识别；"
        "state=loading/paused 表示正在等待语音退出或加载模型。",
        PropertyList({
            Property("enabled", kPropertyTypeBoolean),
        }),
        [](const PropertyList &properties) -> ReturnValue {
            const bool enabled = properties["enabled"].value<bool>();
            if (enabled) {
                if (!gesture_mode_is_active()) {
                    gesture_mode_start(GESTURE_START_MCP);
                }
            } else {
                gesture_mode_stop(GESTURE_STOP_USER);
            }
            return BuildStatus();
        });

    server.AddTool(
        "self.gesture.get_status",
        "读取手势模式的实时状态。用户询问是否开启、是否退出，或说自己已经按下"
        "触屏退出按钮时，必须先调用本工具再回答，不能沿用之前 set_mode 的旧结果。",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            (void)properties;
            return BuildStatus();
        });
}
