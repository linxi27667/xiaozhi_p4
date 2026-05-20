#include "smart_home_mcp_tool.h"

#include "mcp_server.h"
#include "mqtt_device_model.h"
#include "../services/mqtt_iot_protocol.h"
#include "../services/xiaozhi_mqtt.h"

#include <cJSON.h>

#include <cstring>
#include <stdexcept>
#include <string>

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
        cJSON_AddNumberToObject(item, "floor", floor);
        if (device_model_get_rain_value(floor, &value)) {
            cJSON_AddNumberToObject(item, "rain_mv", value);
        }
        if (device_model_get_flame_value(floor, &value)) {
            cJSON_AddNumberToObject(item, "flame_mv", value);
        }
        cJSON_AddItemToArray(floors, item);
    }

    return root;
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
    throw std::runtime_error("Unsupported device type: " + type);
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

    server.AddTool("self.iot.set_gpio",
        "Set a GPIO-like smart-home output. type can be light, relay, servo, or gpio.",
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
            mqtt_send_command(floor, cmd, index, value);
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

    server.AddTool("self.iot.set_servo_by_index",
        "Set a smart-home servo angle.",
        PropertyList({
            Property("floor", kPropertyTypeInteger, 1, 3),
            Property("index", kPropertyTypeInteger, 0, 15),
            Property("angle", kPropertyTypeInteger, 0, 180),
        }),
        [](const PropertyList& properties) -> ReturnValue {
            mqtt_send_command(
                static_cast<uint8_t>(properties["floor"].value<int>()),
                IOT_CMD_SET_SERVO,
                static_cast<uint8_t>(properties["index"].value<int>()),
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
}
