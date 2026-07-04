#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void mqtt_client_init(void);
void mqtt_client_start(void);
void mqtt_client_stop(void);
bool mqtt_client_is_connected(void);
void mqtt_client_poll(void);

/**
 * Send a binary command to a slave device.
 * @param floor_id  1, 2, or 3
 * @param cmd_type  IOT_CMD_SET_LIGHT, IOT_CMD_SET_RELAY, IOT_CMD_SET_SERVO
 * @param gpio_index  logical index on the slave (light/relay array index;
 *                    servo accepts local 0~2 or protocol 6~8)
 * @param value     0/1 for on/off, 0-180 for servo angle
 */
void mqtt_send_command(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value);
void mqtt_send_command_v3(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value, uint8_t source);
void mqtt_send_ambient_scene(uint8_t floor_id, uint8_t index, uint8_t ambient_scene);
void mqtt_send_rgb_light(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                         uint8_t blue, uint8_t brightness, uint8_t effect, uint8_t speed);
void mqtt_send_scene(uint8_t scene_id);
void mqtt_send_main_power(uint8_t floor_id, bool on);
void mqtt_send_all_main_power(bool on);

/**
 * Send a broadcast command to all slaves.
 * @param cmd_type  IOT_CMD_BROADCAST_ALL_OFF, _ALL_ON, _LIGHTS_OFF, _LIGHTS_ON, _EMERGENCY
 */
void mqtt_send_broadcast(uint8_t cmd_type);

#ifdef __cplusplus
}
#endif
