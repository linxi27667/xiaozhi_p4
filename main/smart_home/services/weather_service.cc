#include "weather_service.h"

#include "board.h"
#include "mqtt_device_model.h"
#include "wifi_compat.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <http.h>

#include <memory>
#include <string>
#include <cstdio>

static const char* TAG = "WEATHER";

static constexpr int64_t kFetchIntervalMs = 10 * 60 * 1000;
static constexpr int64_t kRetryIntervalMs = 60 * 1000;

static bool s_started;
static int64_t s_last_attempt_ms;
static bool s_has_success;

typedef struct {
    float temp;
    uint8_t humidity;
    float precipitation;
    float wind_speed;
    uint16_t weather_code;
    float pm25;
    uint16_t aqi;
    bool air_valid;
} weather_sample_t;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool read_json_url(const char* url, std::string* out)
{
    if (!url || !out) return false;

    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGW(TAG, "Network interface not ready");
        return false;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGW(TAG, "CreateHttp failed");
        return false;
    }
    http->SetHeader("Accept", "application/json");
    http->SetHeader("User-Agent", "xiaozhi-smart-home-weather");

    if (!http->Open("GET", url)) {
        ESP_LOGW(TAG, "HTTP open failed, err=0x%x", http->GetLastError());
        return false;
    }

    int status = http->GetStatusCode();
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP status=%d", status);
        http->Close();
        return false;
    }

    *out = http->ReadAll();
    http->Close();
    return !out->empty();
}

static bool json_number(cJSON* object, const char* name, double* value)
{
    cJSON* item = cJSON_GetObjectItem(object, name);
    if (!cJSON_IsNumber(item)) return false;
    if (value) *value = item->valuedouble;
    return true;
}

static bool parse_forecast(const std::string& body, weather_sample_t* sample)
{
    if (!sample) return false;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return false;

    bool ok = false;
    cJSON* current = cJSON_GetObjectItem(root, "current");
    if (cJSON_IsObject(current)) {
        double temp = 0.0;
        double humidity = 0.0;
        double precipitation = 0.0;
        double weather_code = 0.0;
        double wind_speed = 0.0;
        ok = json_number(current, "temperature_2m", &temp) &&
             json_number(current, "relative_humidity_2m", &humidity) &&
             json_number(current, "precipitation", &precipitation) &&
             json_number(current, "weather_code", &weather_code) &&
             json_number(current, "wind_speed_10m", &wind_speed);
        if (ok) {
            int humidity_int = (int)(humidity + 0.5);
            sample->temp = (float)temp;
            if (humidity_int < 0) humidity_int = 0;
            if (humidity_int > 100) humidity_int = 100;
            sample->humidity = (uint8_t)humidity_int;
            sample->precipitation = (float)precipitation;
            sample->weather_code = weather_code < 0 ? 0 : (uint16_t)weather_code;
            sample->wind_speed = (float)wind_speed;
        }
    }

    cJSON_Delete(root);
    return ok;
}

static bool parse_air_quality(const std::string& body, weather_sample_t* sample)
{
    if (!sample) return false;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return false;

    bool ok = false;
    cJSON* current = cJSON_GetObjectItem(root, "current");
    if (cJSON_IsObject(current)) {
        double pm25 = 0.0;
        double aqi = 0.0;
        ok = json_number(current, "pm2_5", &pm25) &&
             json_number(current, "us_aqi", &aqi);
        if (ok) {
            sample->pm25 = (float)pm25;
            sample->aqi = aqi < 0 ? 0 : (uint16_t)aqi;
            sample->air_valid = true;
        }
    }

    cJSON_Delete(root);
    return ok;
}

static bool fetch_weather(void)
{
    weather_sample_t sample = {};
    char forecast_url[256];
    char air_url[256];
    snprintf(forecast_url, sizeof(forecast_url),
        "https://api.open-meteo.com/v1/forecast?"
        "latitude=%s&longitude=%s"
        "&current=temperature_2m,relative_humidity_2m,precipitation,weather_code,wind_speed_10m"
        "&timezone=Asia%%2FShanghai",
        WEATHER_LAT_STR, WEATHER_LON_STR);
    snprintf(air_url, sizeof(air_url),
        "https://air-quality-api.open-meteo.com/v1/air-quality?"
        "latitude=%s&longitude=%s"
        "&current=pm2_5,us_aqi"
        "&timezone=Asia%%2FShanghai",
        WEATHER_LAT_STR, WEATHER_LON_STR);

    std::string body;
    if (!read_json_url(forecast_url, &body) || !parse_forecast(body, &sample)) {
        ESP_LOGW(TAG, "Forecast fetch/parse failed");
        return false;
    }

    body.clear();
    if (!read_json_url(air_url, &body) || !parse_air_quality(body, &sample)) {
        sample.air_valid = false;
        ESP_LOGW(TAG, "Air-quality fetch/parse failed, keeping weather only");
    }

    device_model_update_weather(WEATHER_LOCATION_NAME, sample.temp, sample.humidity,
        sample.precipitation, sample.wind_speed, sample.weather_code,
        sample.pm25, sample.aqi, sample.air_valid);
    return true;
}

extern "C" void weather_service_init(void)
{
    s_started = false;
    s_last_attempt_ms = 0;
    s_has_success = false;
}

extern "C" void weather_service_start(void)
{
    s_started = true;
}

extern "C" void weather_service_poll(void)
{
    if (!s_started || !wifi_manager_is_connected()) {
        return;
    }

    int64_t now = now_ms();
    int64_t interval = s_has_success ? kFetchIntervalMs : kRetryIntervalMs;
    if (s_last_attempt_ms != 0 && now - s_last_attempt_ms < interval) {
        return;
    }
    s_last_attempt_ms = now;

    if (fetch_weather()) {
        s_has_success = true;
        ESP_LOGI(TAG, "Weather updated for %s", WEATHER_LOCATION_NAME);
    }
}
