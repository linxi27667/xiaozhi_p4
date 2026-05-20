#ifndef __MQTT_RECEIVE_H__
#define __MQTT_RECEIVE_H__

#include "mqtt_client.h"

void MQTT_Receive_Init(void);
esp_mqtt_client_handle_t MQTT_Get_Client(void);
const char* MQTT_Get_MAC_Str(void);

#endif
