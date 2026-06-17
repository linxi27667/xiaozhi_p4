#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define WEATHER_LAT 23.1291
#define WEATHER_LON 113.2644
#define WEATHER_LAT_STR "23.1291"
#define WEATHER_LON_STR "113.2644"
#define WEATHER_LOCATION_NAME "广州"
#define WEATHER_TIMEZONE "Asia/Shanghai"

void weather_service_init(void);
void weather_service_start(void);
void weather_service_poll(void);

#ifdef __cplusplus
}
#endif
