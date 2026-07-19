#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define WEATHER_LAT 24.4798
#define WEATHER_LON 118.0894
#define WEATHER_LAT_STR "24.4798"
#define WEATHER_LON_STR "118.0894"
#define WEATHER_LOCATION_NAME "厦门"
#define WEATHER_TIMEZONE "Asia/Shanghai"

void weather_service_init(void);
void weather_service_start(void);
void weather_service_poll(void);

#ifdef __cplusplus
}
#endif
