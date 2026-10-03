#include <assert.h>
#include <math.h>

// Exercise the production static serializers without an HTTP server or API key.
#include "../src/http_server.c"

static void expect_number(const cJSON *object, const char *key, double expected) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    assert(cJSON_IsNumber(value));
    assert(fabs(value->valuedouble - expected) < 0.000001);
}

int main(void) {
    forecast_hour_t hours[2] = {
        {.time_epoch = 1791050400, .time = "2026-10-03 21:00", .temp_c = 22,
         .wind_kph = 12, .humidity = 60, .chance_of_rain = 15, .pressure_mb = 1021.5, .uv = 0},
        {.time_epoch = 1791054000, .time = "2026-10-03 22:00", .temp_c = 25,
         .wind_kph = 18, .humidity = 55, .chance_of_rain = 25, .pressure_mb = 1015.2, .uv = 4.7}
    };
    forecast_daily_t day = {
        .date = "2026-10-03", .date_epoch = 1790974800,
        .day = {.maxtemp_c = 25, .mintemp_c = 18, .uv = 5},
        .hour = hours, .hour_count = 2
    };
    forecast_response_t response = {
        .location = {.name = "Parikia", .region = "South Aegean", .country = "Greece",
                     .lat = 37.0853, .lon = 25.1504, .tz_id = "Europe/Athens",
                     .localtime_epoch = 1791056415, .localtime = "2026-10-03 22:40"},
        .current = {.last_updated_epoch = 1791055800, .last_updated = "2026-10-03 22:30",
                    .temp_c = 22, .pressure_mb = 1021.5, .uv = 0},
        .forecast = &day, .forecast_days = 1
    };
    cJSON *fixtures = cJSON_CreateObject();
    cJSON *hourly = forecast_response_to_json(&response, 1);
    weather_response_t current_response = {.location = response.location, .current = response.current};
    cJSON *current = weather_response_to_json(&current_response);
    assert(cJSON_Compare(cJSON_GetObjectItem(hourly, "location"), cJSON_GetObjectItem(current, "location"), 1));
    assert(cJSON_Compare(cJSON_GetObjectItem(hourly, "current"), cJSON_GetObjectItem(current, "current"), 1));
    cJSON *forecast = cJSON_GetObjectItem(hourly, "forecast");
    cJSON *daily = cJSON_GetArrayItem(cJSON_GetObjectItem(forecast, "forecastday"), 0);
    expect_number(cJSON_GetObjectItem(daily, "day"), "uv", 5);
    cJSON *hour_array = cJSON_GetObjectItem(daily, "hour");
    assert(cJSON_GetArraySize(hour_array) == 2);
    for (int i = 0; i < 2; i++) {
        cJSON *hour = cJSON_GetArrayItem(hour_array, i);
        expect_number(hour, "time_epoch", hours[i].time_epoch);
        expect_number(hour, "temp_c", hours[i].temp_c);
        expect_number(hour, "chance_of_rain", hours[i].chance_of_rain);
        expect_number(hour, "pressure_mb", hours[i].pressure_mb);
        expect_number(hour, "uv", hours[i].uv);
    }
    cJSON_AddItemToObject(fixtures, "hourly", hourly);
    cJSON_Delete(current);

    cJSON *without_hours = forecast_response_to_json(&response, 0);
    daily = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(without_hours, "forecast"), "forecastday"), 0);
    assert(!cJSON_HasObjectItem(daily, "hour"));
    assert(cJSON_HasObjectItem(without_hours, "current"));
    cJSON_AddItemToObject(fixtures, "daily", without_hours);

    response.current.last_updated_epoch = 0;
    cJSON *missing_epoch = forecast_response_to_json(&response, 1);
    assert(!cJSON_HasObjectItem(missing_epoch, "current"));
    cJSON_AddItemToObject(fixtures, "missing_epoch", missing_epoch);
    response.current.last_updated_epoch = 1791055800;
    response.current.last_updated[0] = '\0';
    cJSON *missing_time = forecast_response_to_json(&response, 1);
    assert(!cJSON_HasObjectItem(missing_time, "current"));
    cJSON_AddItemToObject(fixtures, "missing_time", missing_time);

    char *json = cJSON_PrintUnformatted(fixtures);
    assert(json);
    puts(json);
    free(json);
    cJSON_Delete(fixtures);
    return 0;
}
