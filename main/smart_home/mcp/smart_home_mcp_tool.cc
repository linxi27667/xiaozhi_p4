#include "smart_home_mcp_tool.h"

#include "mcp_server.h"
#include "mqtt_device_model.h"
#include "../services/mqtt_iot_protocol.h"
#include "../services/xiaozhi_mqtt.h"

#include <cJSON.h>
#include <esp_log.h>

#include <cstring>
#include <stdexcept>
#include <string>

static const char* TAG = "SH_MCP";

static cJSON* build_status_json() {
    const mqtt_device_model_t* model = device_model_get();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "mqtt_connected", model->mqtt_state == MQTT_STATE_CONNECTED);
    cJSON_AddNumberToObject(root, "device_count", model->device_count);
    cJSON_AddNumberToObject(root, "connected_count", device_model_connected_count());
    cJSON_AddNumberToObject(root, "mqtt_rx_count", model->mqtt_rx_count);
    cJSON_AddNumberToObject(root, "mqtt_last_seen_sec", model->mqtt_last_seen_sec);

    cJSON* controllers = cJSON_AddArrayToObject(root, "controllers");
    for (int floor = 1; floor <= 3; floor++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "floor", floor);
        cJSON_AddBoolToObject(item, "online", model->controller_online[floor - 1]);
        cJSON_AddItemToArray(controllers, item);
    }

    cJSON* devices = cJSON_AddArrayToObject(root, "devices");
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t* d = device_model_at(i);
        if (!d) {
            continue;
        }
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", d->id);
        cJSON_AddStringToObject(item, "name", d->name);
        cJSON_AddStringToObject(item, "floor", device_model_floor_name(d->floor));
        cJSON_AddStringToObject(item, "type", device_model_type_name(d->type));
        cJSON_AddBoolToObject(item, "online", d->connected);
        cJSON_AddBoolToObject(item, "power_on", d->power_on);
        cJSON_AddNumberToObject(item, "value", d->value);
        cJSON_AddStringToObject(item, "value_text", d->value_text);
        cJSON_AddNumberToObject(item, "floor_id", d->floor_id);
        cJSON_AddNumberToObject(item, "cmd_type", d->cmd_type);
        cJSON_AddNumberToObject(item, "gpio_index", d->gpio_index);
        cJSON_AddItemToArray(devices, item);
    }
    return root;
}

static cJSON* build_sensors_json() {
    const mqtt_device_model_t* model = device_model_get();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "sensor_rx_count", model->sensor_rx_count);
    cJSON_AddNumberToObject(root, "online_count", device_model_sensor_online_count());

    cJSON* floors = cJSON_AddArrayToObject(root, "floors");
    for (uint8_t floor = 1; floor <= 3; floor++) {
        cJSON* item = cJSON_CreateObject();
        uint16_t value = 0;
        uint8_t status = 0;
        cJSON_AddNumberToObject(item, "floor", floor);
        if (device_model_get_flame_sensor_mv(floor, &value)) {
            cJSON_AddNumberToObject(item, "flame_mv", value);
        }
        if (device_model_get_rain_value(floor, &value)) {
            cJSON_AddNumberToObject(item, "rain_mv", value);
        }
        if (device_model_get_fire_status(floor, &status)) {
            cJSON_AddNumberToObject(item, "fire_status", status);
            cJSON_AddBoolToObject(item, "fire_alarm", status >= 2);
        }
        if (device_model_get_rain_status(floor, &status)) {
            cJSON_AddNumberToObject(item, "rain_status", status);
        }
        cJSON_AddItemToArray(floors, item);
    }

    return root;
}

static const rc_device_t* find_device_by_id(const std::string& device_id) {
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t* d = device_model_at(i);
        if (d && device_id == d->id) {
            return d;
        }
    }
    return nullptr;
}

static cJSON* build_servo_list_json() {
    cJSON* root = cJSON_CreateObject();
    cJSON* devices = cJSON_AddArrayToObject(root, "servos");

    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t* d = device_model_at(i);
        if (!d || d->cmd_type != IOT_CMD_SET_SERVO) {
            continue;
        }

        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "device_id", d->id);
        cJSON_AddStringToObject(item, "name", d->name);
        cJSON_AddStringToObject(item, "floor", device_model_floor_name(d->floor));
        cJSON_AddNumberToObject(item, "floor_id", d->floor_id);
        cJSON_AddNumberToObject(item, "protocol_index", d->gpio_index);
        cJSON_AddNumberToObject(item, "open_angle", d->servo_open_angle);
        cJSON_AddNumberToObject(item, "close_angle", d->servo_close_angle);
        cJSON_AddBoolToObject(item, "online", d->connected);
        cJSON_AddBoolToObject(item, "open", d->power_on);
        cJSON_AddNumberToObject(item, "current_angle", d->value);
        cJSON_AddStringToObject(item, "value_text", d->value_text);
        cJSON_AddItemToArray(devices, item);
    }

    return root;
}

static cJSON* build_servo_command_result(const rc_device_t* d, uint8_t requested_angle,
                                         uint8_t sent_angle) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "mqtt_connected", mqtt_client_is_connected());
    cJSON_AddStringToObject(root, "device_id", d->id);
    cJSON_AddStringToObject(root, "name", d->name);
    cJSON_AddStringToObject(root, "floor", device_model_floor_name(d->floor));
    cJSON_AddNumberToObject(root, "floor_id", d->floor_id);
    cJSON_AddNumberToObject(root, "protocol_index", d->gpio_index);
    cJSON_AddNumberToObject(root, "requested_angle", requested_angle);
    cJSON_AddNumberToObject(root, "sent_angle", sent_angle);
    cJSON_AddNumberToObject(root, "open_angle", d->servo_open_angle);
    cJSON_AddNumberToObject(root, "close_angle", d->servo_close_angle);

    uint8_t rain_status = 0;
    bool rain_locked = device_model_get_rain_status(3, &rain_status) && rain_status != 0 &&
                       (strcmp(d->id, "floor2_hanger") == 0 ||
                        strcmp(d->id, "floor3_hanger") == 0) &&
                       sent_angle == 0 && requested_angle > 0;
    cJSON_AddBoolToObject(root, "rain_locked", rain_locked);
    return root;
}

static cJSON* send_servo_device(const std::string& device_id, uint8_t angle) {
    const rc_device_t* d = find_device_by_id(device_id);
    if (!d) {
        throw std::runtime_error("Unknown servo device_id: " + device_id);
    }
    if (d->cmd_type != IOT_CMD_SET_SERVO) {
        throw std::runtime_error("Device is not a servo: " + device_id);
    }

    uint8_t sent_angle = angle;
    uint8_t rain_status = 0;
    if (device_model_get_rain_status(3, &rain_status) && rain_status != 0 &&
        (strcmp(d->id, "floor2_hanger") == 0 || strcmp(d->id, "floor3_hanger") == 0) &&
        sent_angle > 0) {
        sent_angle = 0;
    }

    if (!mqtt_send_command(d->floor_id, IOT_CMD_SET_SERVO, d->gpio_index, sent_angle)) {
        throw std::runtime_error("MQTT publish failed for servo: " + device_id);
    }
    return build_servo_command_result(d, angle, sent_angle);
}

static uint8_t command_from_type(const std::string& type) {
    if (type == "light") {
        return IOT_CMD_SET_LIGHT;
    }
    if (type == "relay") {
        return IOT_CMD_SET_RELAY;
    }
    if (type == "servo") {
        return IOT_CMD_SET_SERVO;
    }
    if (type == "gpio") {
        return IOT_CMD_SET_GPIO;
    }
    if (type == "main_power" || type == "power") {
        return IOT_CMD_SET_MAIN_POWER;
    }
    throw std::runtime_error("Unsupported device type: " + type);
}

static uint8_t canonical_servo_index(uint8_t index) {
    return (index < 6) ? static_cast<uint8_t>(index + 6) : index;
}

static uint8_t scene_id_from_name(const std::string& scene_name) {
    struct SceneNameMap {
        const char* name;
        uint8_t id;
    };
    static constexpr SceneNameMap kSceneNames[] = {
        {"sleep", IOT_SCENE_SLEEP}, {"睡眠", IOT_SCENE_SLEEP}, {"睡眠场景", IOT_SCENE_SLEEP},
        {"movie", IOT_SCENE_MOVIE}, {"观影", IOT_SCENE_MOVIE}, {"观影场景", IOT_SCENE_MOVIE},
        {"night", IOT_SCENE_NIGHT}, {"起夜", IOT_SCENE_NIGHT}, {"起夜场景", IOT_SCENE_NIGHT},
        {"夜间", IOT_SCENE_NIGHT},
        {"fire", IOT_SCENE_FIRE}, {"fire_demo", IOT_SCENE_FIRE}, {"火警", IOT_SCENE_FIRE},
        {"火警场景", IOT_SCENE_FIRE},
        {"rain", IOT_SCENE_RAIN}, {"雨天", IOT_SCENE_RAIN}, {"雨天收衣", IOT_SCENE_RAIN},
        {"雨天场景", IOT_SCENE_RAIN},
        {"away", IOT_SCENE_AWAY}, {"离家", IOT_SCENE_AWAY}, {"离家场景", IOT_SCENE_AWAY},
        {"home", IOT_SCENE_HOME}, {"回家", IOT_SCENE_HOME}, {"回家场景", IOT_SCENE_HOME},
        {"bright", IOT_SCENE_BRIGHT}, {"明亮", IOT_SCENE_BRIGHT}, {"明亮场景", IOT_SCENE_BRIGHT},
    };

    for (const auto& scene : kSceneNames) {
        if (scene_name == scene.name) {
            return scene.id;
        }
    }
    throw std::runtime_error("Unknown scene_name: " + scene_name);
}

extern "C" void SmartHomeMcp_RegisterTools(void) {
    auto& server = McpServer::GetInstance();

    server.AddTool("self.iot.get_status",
        "Get smart-home controller, device, and MQTT status.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            return build_status_json();
        });

    server.AddTool("self.iot.get_sensors",
        "Get smart-home sensor readings reported by slave controllers.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            return build_sensors_json();
        });

    server.AddTool("self.iot.discover",
        "Ask all smart-home slave controllers to announce themselves.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            mqtt_send_broadcast(IOT_CMD_DISCOVER);
            return true;
        });

    server.AddUserOnlyTool("self.iot.set_gpio",
        "Set a GPIO-like smart-home output. type can be light, relay, servo, gpio, or main_power. For Chinese/natural servo commands prefer self.iot.set_servo_power. Servo index accepts local 0-2 or protocol 6-8: da men/front gate=1F index 6, 2F hanger=8, 3F right skylight=6, 3F left skylight=7, 3F hanger=8.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("type", kPropertyTypeString, "light"),
            Property("index", kPropertyTypeInteger, 0, 15),
            Property("value", kPropertyTypeInteger, 0, 180),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            uint8_t floor = static_cast<uint8_t>(properties["floor"].value<int>());
            uint8_t cmd = command_from_type(properties["type"].value<std::string>());
            uint8_t index = static_cast<uint8_t>(properties["index"].value<int>());
            uint8_t value = static_cast<uint8_t>(properties["value"].value<int>());
            if (cmd == IOT_CMD_SET_SERVO) {
                index = canonical_servo_index(index);
            }
            mqtt_send_command(floor, cmd, index, value);
            return true;
        });

    server.AddUserOnlyTool("self.iot.list_servos",
        "List smart-home servo devices and their stable device_id values, protocol indexes, open angles, close angles, and current state.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            return build_servo_list_json();
        });

    server.AddUserOnlyTool("self.iot.set_servo",
        "Set a smart-home servo by stable device_id instead of guessing indexes. Device mapping: da men/front gate/floor1_gate, er lou liangyigan/floor2_hanger, san lou you tianchuang/floor3_right_skylight, san lou zuo tianchuang/floor3_left_skylight, san lou liangyigan/floor3_hanger. Rain status locks floor2_hanger and floor3_hanger closed.",
        PropertyList({
            Property("device_id", kPropertyTypeString),
            Property("angle", kPropertyTypeInteger, 0, 180),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            return send_servo_device(
                properties["device_id"].value<std::string>(),
                static_cast<uint8_t>(properties["angle"].value<int>()));
        });

    server.AddTool("self.iot.set_servo_power",
        "The only AI-visible servo control tool. Call it directly; do not call discover, get_status, list_servos, set_gpio, or angle tools first. Mapping: open gate/开门/打开大门 => floor1_gate open=true; close gate/关门 => floor1_gate open=false; 2F hanger/二楼晾衣架 => floor2_hanger; 3F right skylight/三楼右天窗 => floor3_right_skylight; 3F left skylight/三楼左天窗 => floor3_left_skylight; 3F hanger/三楼晾衣架 => floor3_hanger. Use open=true to open or extend and open=false to close or retract. Rain locks both hangers closed.",
        PropertyList({
            Property("device_id", kPropertyTypeString),
            Property("open", kPropertyTypeBoolean),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            std::string device_id = properties["device_id"].value<std::string>();
            const rc_device_t* d = find_device_by_id(device_id);
            if (!d) {
                throw std::runtime_error("Unknown servo device_id: " + device_id);
            }
            if (d->cmd_type != IOT_CMD_SET_SERVO) {
                throw std::runtime_error("Device is not a servo: " + device_id);
            }
            bool open = properties["open"].value<bool>();
            uint8_t angle = open ?
                d->servo_open_angle : d->servo_close_angle;
            ESP_LOGI(TAG, "Servo command: device_id=%s open=%s floor=%u index=%u angle=%u",
                     d->id, open ? "true" : "false", d->floor_id, d->gpio_index, angle);
            return send_servo_device(device_id, angle);
        });

    server.AddTool("self.iot.set_main_power",
        "Turn one floor's main power breaker on or off.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("on", kPropertyTypeBoolean),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_main_power(
                static_cast<uint8_t>(properties["floor"].value<int>()),
                properties["on"].value<bool>());
            return true;
        });

    server.AddTool("self.iot.set_all_main_power",
        "Turn all floor main power breakers on or off.",
        PropertyList({
            Property("on", kPropertyTypeBoolean),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_all_main_power(properties["on"].value<bool>());
            return true;
        });

    server.AddTool("self.iot.set_light",
        "Turn a smart-home light on or off.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("index", kPropertyTypeInteger, 0, 15),
            Property("on", kPropertyTypeBoolean),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_command(
                static_cast<uint8_t>(properties["floor"].value<int>()),
                IOT_CMD_SET_LIGHT,
                static_cast<uint8_t>(properties["index"].value<int>()),
                properties["on"].value<bool>() ? 1 : 0);
            return true;
        });

    server.AddTool("self.iot.set_rgb_light",
        "Set the 2F master bedroom RGB ambient light color, brightness, and effect.",
        PropertyList({
            Property("red", kPropertyTypeInteger, 0, 255),
            Property("green", kPropertyTypeInteger, 0, 255),
            Property("blue", kPropertyTypeInteger, 0, 255),
            Property("brightness", kPropertyTypeInteger, 0, 100),
            Property("effect", kPropertyTypeInteger, 0, 3),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_rgb_light(
                2,
                0,
                static_cast<uint8_t>(properties["red"].value<int>()),
                static_cast<uint8_t>(properties["green"].value<int>()),
                static_cast<uint8_t>(properties["blue"].value<int>()),
                static_cast<uint8_t>(properties["brightness"].value<int>()),
                static_cast<uint8_t>(properties["effect"].value<int>()),
                16);
            return true;
        });

    server.AddTool("self.iot.set_scene",
        "Run exactly one smart-home scene. scene_name must be: night for 起夜/夜间; home for 回家; "
        "away for 离家; sleep for 睡眠; movie for 观影; fire for 火警演示; rain for 雨天收衣; "
        "bright for 明亮. Important: 起夜 always uses scene_name=night, never home or away.",
        PropertyList({
            Property("scene_name", kPropertyTypeString),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            const std::string scene_name = properties["scene_name"].value<std::string>();
            const uint8_t scene_id = scene_id_from_name(scene_name);
            ESP_LOGI(TAG, "Scene command: name=%s id=%u", scene_name.c_str(), scene_id);
            mqtt_send_scene(scene_id);
            return true;
        });

    server.AddUserOnlyTool("self.iot.set_scene_by_id",
        "Run a smart-home scene by protocol ID for manual debugging: 1 sleep, 2 movie, 3 night, "
        "4 fire demo, 5 rain, 6 away, 7 home, 8 bright.",
        PropertyList({
            Property("scene_id", kPropertyTypeInteger, 1, 8),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_scene(static_cast<uint8_t>(properties["scene_id"].value<int>()));
            return true;
        });

    server.AddTool("self.iot.set_relay",
        "Turn a smart-home relay on or off.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("index", kPropertyTypeInteger, 0, 15),
            Property("on", kPropertyTypeBoolean),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_command(
                static_cast<uint8_t>(properties["floor"].value<int>()),
                IOT_CMD_SET_RELAY,
                static_cast<uint8_t>(properties["index"].value<int>()),
                properties["on"].value<bool>() ? 1 : 0);
            return true;
        });

    server.AddUserOnlyTool("self.iot.set_servo_by_index",
        "Set a smart-home servo angle by floor/index for protocol debugging. Prefer self.iot.set_servo_power for Chinese/natural language control. Index accepts local servo 0-2 or protocol index 6-8: da men/front gate=1F index 6, 2F hanger=8, 3F right skylight=6, 3F left skylight=7, 3F hanger=8.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("index", kPropertyTypeInteger, 0, 15),
            Property("angle", kPropertyTypeInteger, 0, 180),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_command(
                static_cast<uint8_t>(properties["floor"].value<int>()),
                IOT_CMD_SET_SERVO,
                canonical_servo_index(static_cast<uint8_t>(properties["index"].value<int>())),
                static_cast<uint8_t>(properties["angle"].value<int>()));
            return true;
        });

    server.AddTool("self.iot.all_off",
        "Turn off all smart-home outputs.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            mqtt_send_broadcast(IOT_CMD_BROADCAST_ALL_OFF);
            return true;
        });

    server.AddTool("self.iot.all_on",
        "Turn on all smart-home outputs.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            mqtt_send_broadcast(IOT_CMD_BROADCAST_ALL_ON);
            return true;
        });

    server.AddTool("self.iot.all_lights_off",
        "Turn off all smart-home lights.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF);
            return true;
        });

    server.AddTool("self.iot.all_lights_on",
        "Turn on all smart-home lights.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
            return true;
        });

    ESP_LOGI(TAG, "AI-visible servo writer: self.iot.set_servo_power");
}
