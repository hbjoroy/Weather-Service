#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "weather_api.h"
#include "http_client.h"

static weather_config_t api_config;
static int api_initialized = 0;

/**
 * Helper function to safely extract string from JSON object
 */
static void json_get_string(const cJSON *json, const char *key, char *dest, size_t dest_size) {
    const cJSON *item = cJSON_GetObjectItem(json, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dest, item->valuestring, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else {
        dest[0] = '\0';
    }
}

/**
 * Helper function to safely extract double from JSON object
 */
static double json_get_double(const cJSON *json, const char *key) {
    const cJSON *item = cJSON_GetObjectItem(json, key);
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return 0.0;
}

/**
 * Helper function to safely extract integer from JSON object
 */
static int json_get_int(const cJSON *json, const char *key) {
    const cJSON *item = cJSON_GetObjectItem(json, key);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return 0;
}

/**
 * Helper function to safely extract long from JSON object
 */
static long json_get_long(const cJSON *json, const char *key) {
    const cJSON *item = cJSON_GetObjectItem(json, key);
    if (cJSON_IsNumber(item)) {
        return (long)item->valuedouble;
    }
    return 0;
}

/**
 * Parse location data from JSON
 */
static void parse_location(const cJSON *location_json, location_t *location) {
    if (!location_json || !location) return;
    
    json_get_string(location_json, "name", location->name, sizeof(location->name));
    json_get_string(location_json, "region", location->region, sizeof(location->region));
    json_get_string(location_json, "country", location->country, sizeof(location->country));
    location->lat = json_get_double(location_json, "lat");
    location->lon = json_get_double(location_json, "lon");
    json_get_string(location_json, "tz_id", location->tz_id, sizeof(location->tz_id));
    location->localtime_epoch = json_get_long(location_json, "localtime_epoch");
    json_get_string(location_json, "localtime", location->localtime, sizeof(location->localtime));
}

/**
 * Parse weather condition from JSON
 */
static void parse_condition(const cJSON *condition_json, weather_condition_t *condition) {
    if (!condition_json || !condition) return;
    
    json_get_string(condition_json, "text", condition->text, sizeof(condition->text));
    json_get_string(condition_json, "icon", condition->icon, sizeof(condition->icon));
    condition->code = json_get_int(condition_json, "code");
}

/**
 * Parse current weather data from JSON
 */
static void parse_current_weather(const cJSON *current_json, current_weather_t *current) {
    if (!current_json || !current) return;
    
    current->last_updated_epoch = json_get_long(current_json, "last_updated_epoch");
    json_get_string(current_json, "last_updated", current->last_updated, sizeof(current->last_updated));
    current->temp_c = json_get_double(current_json, "temp_c");
    current->temp_f = json_get_double(current_json, "temp_f");
    current->is_day = json_get_int(current_json, "is_day");
    
    // Parse condition
    const cJSON *condition_json = cJSON_GetObjectItem(current_json, "condition");
    parse_condition(condition_json, &current->condition);
    
    current->wind_mph = json_get_double(current_json, "wind_mph");
    current->wind_kph = json_get_double(current_json, "wind_kph");
    current->wind_degree = json_get_int(current_json, "wind_degree");
    json_get_string(current_json, "wind_dir", current->wind_dir, sizeof(current->wind_dir));
    current->pressure_mb = json_get_double(current_json, "pressure_mb");
    current->pressure_in = json_get_double(current_json, "pressure_in");
    current->precip_mm = json_get_double(current_json, "precip_mm");
    current->precip_in = json_get_double(current_json, "precip_in");
    current->humidity = json_get_int(current_json, "humidity");
    current->cloud = json_get_int(current_json, "cloud");
    current->feelslike_c = json_get_double(current_json, "feelslike_c");
    current->feelslike_f = json_get_double(current_json, "feelslike_f");
    current->windchill_c = json_get_double(current_json, "windchill_c");
    current->windchill_f = json_get_double(current_json, "windchill_f");
    current->heatindex_c = json_get_double(current_json, "heatindex_c");
    current->heatindex_f = json_get_double(current_json, "heatindex_f");
    current->dewpoint_c = json_get_double(current_json, "dewpoint_c");
    current->dewpoint_f = json_get_double(current_json, "dewpoint_f");
    current->vis_km = json_get_double(current_json, "vis_km");
    current->vis_miles = json_get_double(current_json, "vis_miles");
    current->uv = json_get_double(current_json, "uv");
    current->gust_mph = json_get_double(current_json, "gust_mph");
    current->gust_kph = json_get_double(current_json, "gust_kph");
    current->short_rad = json_get_double(current_json, "short_rad");
    current->diff_rad = json_get_double(current_json, "diff_rad");
    current->dni = json_get_double(current_json, "dni");
    current->gti = json_get_double(current_json, "gti");
}

int weather_api_init(const weather_config_t *config) {
    if (!config) {
        fprintf(stderr, "Invalid configuration provided to weather_api_init\n");
        return -1;
    }
    
    if (http_client_init() != 0) {
        fprintf(stderr, "Failed to initialize HTTP client\n");
        return -1;
    }
    
    // Copy configuration
    memcpy(&api_config, config, sizeof(weather_config_t));
    api_initialized = 1;
    
    return 0;
}

void weather_api_cleanup(void) {
    if (api_initialized) {
        http_client_cleanup();
        api_initialized = 0;
    }
}

int weather_api_get_current(const char *location, int include_aqi, weather_response_t *response) {
    if (!api_initialized) {
        fprintf(stderr, "Weather API not initialized. Call weather_api_init() first.\n");
        return -1;
    }
    
    if (!location || !response) {
        fprintf(stderr, "Invalid arguments to weather_api_get_current\n");
        return -1;
    }
    
    // URL encode the location parameter
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl for URL encoding\n");
        return -1;
    }
    
    char *encoded_location = curl_easy_escape(curl, location, 0);
    if (!encoded_location) {
        fprintf(stderr, "Failed to URL encode location\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Build URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/current.json?key=%s&q=%s&aqi=%s",
             api_config.base_url,
             api_config.api_key,
             encoded_location,
             include_aqi ? "yes" : "no");
    
    // Clean up encoded location
    curl_free(encoded_location);
    curl_easy_cleanup(curl);
    
    // Make HTTP request
    http_response_t http_response;
    if (http_get(url, &http_response) != 0) {
        fprintf(stderr, "Failed to make HTTP request\n");
        return -1;
    }
    
    // Check HTTP status
    if (http_response.status_code != 200) {
        fprintf(stderr, "HTTP request failed with status: %ld\n", http_response.status_code);
        if (http_response.data) {
            fprintf(stderr, "Response: %s\n", http_response.data);
        }
        http_response_free(&http_response);
        return -1;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(http_response.data);
    if (!json) {
        fprintf(stderr, "Failed to parse JSON response\n");
        http_response_free(&http_response);
        return -1;
    }
    
    // Clear response structure
    memset(response, 0, sizeof(weather_response_t));
    
    // Parse location
    const cJSON *location_json = cJSON_GetObjectItem(json, "location");
    if (location_json) {
        parse_location(location_json, &response->location);
    }
    
    // Parse current weather
    const cJSON *current_json = cJSON_GetObjectItem(json, "current");
    if (current_json) {
        parse_current_weather(current_json, &response->current);
    }
    
    // Cleanup
    cJSON_Delete(json);
    http_response_free(&http_response);
    
    return 0;
}

void weather_response_free(weather_response_t *response) {
    // Currently no dynamic memory allocation in response structure
    // This function is provided for future extensibility
    if (response) {
        memset(response, 0, sizeof(weather_response_t));
    }
}

void weather_print_current(const weather_response_t *response) {
    if (!response) return;
    
    const location_t *loc = &response->location;
    const current_weather_t *cur = &response->current;
    
    printf("=== Weather Information ===\n");
    printf("Location: %s, %s, %s\n", loc->name, loc->region, loc->country);
    printf("Coordinates: %.4f, %.4f\n", loc->lat, loc->lon);
    printf("Local Time: %s\n", loc->localtime);
    printf("Timezone: %s\n\n", loc->tz_id);
    
    printf("=== Current Conditions ===\n");
    printf("Condition: %s\n", cur->condition.text);
    printf("Temperature: %.1f°C (%.1f°F)\n", cur->temp_c, cur->temp_f);
    printf("Feels Like: %.1f°C (%.1f°F)\n", cur->feelslike_c, cur->feelslike_f);
    printf("Humidity: %d%%\n", cur->humidity);
    printf("Wind: %.1f kph (%.1f mph) %s\n", cur->wind_kph, cur->wind_mph, cur->wind_dir);
    printf("Pressure: %.1f mb (%.2f in)\n", cur->pressure_mb, cur->pressure_in);
    printf("Visibility: %.1f km (%.1f miles)\n", cur->vis_km, cur->vis_miles);
    printf("UV Index: %.1f\n", cur->uv);
    printf("Cloud Cover: %d%%\n", cur->cloud);
    
    if (cur->precip_mm > 0 || cur->precip_in > 0) {
        printf("Precipitation: %.1f mm (%.2f in)\n", cur->precip_mm, cur->precip_in);
    }
    
    printf("Last Updated: %s\n", cur->last_updated);
}

/**
 * Parse astronomy data from JSON
 */
static void parse_astronomy(const cJSON *astro_json, astronomy_t *astro) {
    if (!astro_json || !astro) return;
    
    json_get_string(astro_json, "sunrise", astro->sunrise, sizeof(astro->sunrise));
    json_get_string(astro_json, "sunset", astro->sunset, sizeof(astro->sunset));
    json_get_string(astro_json, "moonrise", astro->moonrise, sizeof(astro->moonrise));
    json_get_string(astro_json, "moonset", astro->moonset, sizeof(astro->moonset));
    json_get_string(astro_json, "moon_phase", astro->moon_phase, sizeof(astro->moon_phase));
    astro->moon_illumination = json_get_int(astro_json, "moon_illumination");
}

/**
 * Parse forecast day data from JSON
 */
static void parse_forecast_day(const cJSON *day_json, forecast_day_t *day) {
    if (!day_json || !day) return;
    
    day->maxtemp_c = json_get_double(day_json, "maxtemp_c");
    day->maxtemp_f = json_get_double(day_json, "maxtemp_f");
    day->mintemp_c = json_get_double(day_json, "mintemp_c");
    day->mintemp_f = json_get_double(day_json, "mintemp_f");
    day->avgtemp_c = json_get_double(day_json, "avgtemp_c");
    day->avgtemp_f = json_get_double(day_json, "avgtemp_f");
    day->maxwind_mph = json_get_double(day_json, "maxwind_mph");
    day->maxwind_kph = json_get_double(day_json, "maxwind_kph");
    day->totalprecip_mm = json_get_double(day_json, "totalprecip_mm");
    day->totalprecip_in = json_get_double(day_json, "totalprecip_in");
    day->totalsnow_cm = json_get_double(day_json, "totalsnow_cm");
    day->avgvis_km = json_get_double(day_json, "avgvis_km");
    day->avgvis_miles = json_get_double(day_json, "avgvis_miles");
    day->avghumidity = json_get_int(day_json, "avghumidity");
    day->daily_will_it_rain = json_get_int(day_json, "daily_will_it_rain");
    day->daily_chance_of_rain = json_get_int(day_json, "daily_chance_of_rain");
    day->daily_will_it_snow = json_get_int(day_json, "daily_will_it_snow");
    day->daily_chance_of_snow = json_get_int(day_json, "daily_chance_of_snow");
    day->uv = json_get_double(day_json, "uv");
    
    // Parse condition
    const cJSON *condition_json = cJSON_GetObjectItem(day_json, "condition");
    parse_condition(condition_json, &day->condition);
}

/**
 * Parse hourly forecast data from JSON
 */
static void parse_forecast_hour(const cJSON *hour_json, forecast_hour_t *hour) {
    if (!hour_json || !hour) return;
    
    hour->time_epoch = json_get_long(hour_json, "time_epoch");
    json_get_string(hour_json, "time", hour->time, sizeof(hour->time));
    hour->temp_c = json_get_double(hour_json, "temp_c");
    hour->temp_f = json_get_double(hour_json, "temp_f");
    hour->is_day = json_get_int(hour_json, "is_day");
    
    // Parse condition
    const cJSON *condition_json = cJSON_GetObjectItem(hour_json, "condition");
    parse_condition(condition_json, &hour->condition);
    
    hour->wind_mph = json_get_double(hour_json, "wind_mph");
    hour->wind_kph = json_get_double(hour_json, "wind_kph");
    hour->wind_degree = json_get_int(hour_json, "wind_degree");
    json_get_string(hour_json, "wind_dir", hour->wind_dir, sizeof(hour->wind_dir));
    hour->pressure_mb = json_get_double(hour_json, "pressure_mb");
    hour->pressure_in = json_get_double(hour_json, "pressure_in");
    hour->precip_mm = json_get_double(hour_json, "precip_mm");
    hour->precip_in = json_get_double(hour_json, "precip_in");
    hour->humidity = json_get_int(hour_json, "humidity");
    hour->cloud = json_get_int(hour_json, "cloud");
    hour->feelslike_c = json_get_double(hour_json, "feelslike_c");
    hour->feelslike_f = json_get_double(hour_json, "feelslike_f");
    hour->windchill_c = json_get_double(hour_json, "windchill_c");
    hour->windchill_f = json_get_double(hour_json, "windchill_f");
    hour->heatindex_c = json_get_double(hour_json, "heatindex_c");
    hour->heatindex_f = json_get_double(hour_json, "heatindex_f");
    hour->dewpoint_c = json_get_double(hour_json, "dewpoint_c");
    hour->dewpoint_f = json_get_double(hour_json, "dewpoint_f");
    hour->will_it_rain = json_get_int(hour_json, "will_it_rain");
    hour->chance_of_rain = json_get_int(hour_json, "chance_of_rain");
    hour->will_it_snow = json_get_int(hour_json, "will_it_snow");
    hour->chance_of_snow = json_get_int(hour_json, "chance_of_snow");
    hour->vis_km = json_get_double(hour_json, "vis_km");
    hour->vis_miles = json_get_double(hour_json, "vis_miles");
    hour->gust_mph = json_get_double(hour_json, "gust_mph");
    hour->gust_kph = json_get_double(hour_json, "gust_kph");
    hour->uv = json_get_double(hour_json, "uv");
}

int weather_api_get_forecast(const char *location, int days, int include_aqi, int include_alerts, forecast_response_t *response) {
    if (!api_initialized) {
        fprintf(stderr, "Weather API not initialized. Call weather_api_init() first.\n");
        return -1;
    }
    
    if (!location || !response) {
        fprintf(stderr, "Invalid arguments to weather_api_get_forecast\n");
        return -1;
    }
    
    if (days < 1 || days > 14) {
        fprintf(stderr, "Invalid forecast days: %d. Must be between 1 and 14.\n", days);
        return -1;
    }
    
    // URL encode the location parameter
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl for URL encoding\n");
        return -1;
    }
    
    char *encoded_location = curl_easy_escape(curl, location, 0);
    if (!encoded_location) {
        fprintf(stderr, "Failed to URL encode location\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Build URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/forecast.json?key=%s&q=%s&days=%d&aqi=%s&alerts=%s",
             api_config.base_url,
             api_config.api_key,
             encoded_location,
             days,
             include_aqi ? "yes" : "no",
             include_alerts ? "yes" : "no");
    
    // Clean up encoded location
    curl_free(encoded_location);
    curl_easy_cleanup(curl);
    
    // Make HTTP request
    http_response_t http_response;
    if (http_get(url, &http_response) != 0) {
        fprintf(stderr, "Failed to make HTTP request\n");
        return -1;
    }
    
    // Check HTTP status
    if (http_response.status_code != 200) {
        fprintf(stderr, "HTTP request failed with status: %ld\n", http_response.status_code);
        if (http_response.data) {
            fprintf(stderr, "Response: %s\n", http_response.data);
        }
        http_response_free(&http_response);
        return -1;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(http_response.data);
    if (!json) {
        fprintf(stderr, "Failed to parse JSON response\n");
        http_response_free(&http_response);
        return -1;
    }
    
    // Clear response structure
    memset(response, 0, sizeof(forecast_response_t));
    
    // Parse location
    const cJSON *location_json = cJSON_GetObjectItem(json, "location");
    if (location_json) {
        parse_location(location_json, &response->location);
    }
    
    // Parse current weather
    const cJSON *current_json = cJSON_GetObjectItem(json, "current");
    if (current_json) {
        parse_current_weather(current_json, &response->current);
    }
    
    // Parse forecast
    const cJSON *forecast_json = cJSON_GetObjectItem(json, "forecast");
    if (forecast_json) {
        const cJSON *forecastday_array = cJSON_GetObjectItem(forecast_json, "forecastday");
        if (cJSON_IsArray(forecastday_array)) {
            int array_size = cJSON_GetArraySize(forecastday_array);
            if (array_size > 0) {
                response->forecast = malloc(array_size * sizeof(forecast_daily_t));
                if (!response->forecast) {
                    fprintf(stderr, "Failed to allocate memory for forecast days\n");
                    cJSON_Delete(json);
                    http_response_free(&http_response);
                    return -1;
                }
                
                response->forecast_days = array_size;
                
                for (int i = 0; i < array_size; i++) {
                    const cJSON *day_json = cJSON_GetArrayItem(forecastday_array, i);
                    if (day_json) {
                        forecast_daily_t *daily = &response->forecast[i];
                        memset(daily, 0, sizeof(forecast_daily_t));
                        
                        // Parse date
                        json_get_string(day_json, "date", daily->date, sizeof(daily->date));
                        daily->date_epoch = json_get_long(day_json, "date_epoch");
                        
                        // Parse day data
                        const cJSON *day_data = cJSON_GetObjectItem(day_json, "day");
                        if (day_data) {
                            parse_forecast_day(day_data, &daily->day);
                        }
                        
                        // Parse astronomy
                        const cJSON *astro_json = cJSON_GetObjectItem(day_json, "astro");
                        if (astro_json) {
                            parse_astronomy(astro_json, &daily->astro);
                        }
                        
                        // Parse hourly data
                        const cJSON *hour_array = cJSON_GetObjectItem(day_json, "hour");
                        if (cJSON_IsArray(hour_array)) {
                            int hour_count = cJSON_GetArraySize(hour_array);
                            if (hour_count > 0) {
                                daily->hour = malloc(hour_count * sizeof(forecast_hour_t));
                                if (daily->hour) {
                                    daily->hour_count = hour_count;
                                    for (int h = 0; h < hour_count; h++) {
                                        const cJSON *hour_json = cJSON_GetArrayItem(hour_array, h);
                                        if (hour_json) {
                                            parse_forecast_hour(hour_json, &daily->hour[h]);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Cleanup
    cJSON_Delete(json);
    http_response_free(&http_response);
    
    return 0;
}

void forecast_response_free(forecast_response_t *response) {
    if (response) {
        if (response->forecast) {
            for (int i = 0; i < response->forecast_days; i++) {
                if (response->forecast[i].hour) {
                    free(response->forecast[i].hour);
                }
            }
            free(response->forecast);
        }
        memset(response, 0, sizeof(forecast_response_t));
    }
}

void weather_print_forecast(const forecast_response_t *response, int show_hourly) {
    if (!response) return;
    
    const location_t *loc = &response->location;
    
    printf("=== Weather Forecast ===\n");
    printf("Location: %s, %s, %s\n", loc->name, loc->region, loc->country);
    printf("Coordinates: %.4f, %.4f\n", loc->lat, loc->lon);
    printf("Timezone: %s\n", loc->tz_id);
    printf("Forecast for %d day(s)\n\n", response->forecast_days);
    
    for (int i = 0; i < response->forecast_days; i++) {
        const forecast_daily_t *daily = &response->forecast[i];
        const forecast_day_t *day = &daily->day;
        
        printf("=== Day %d: %s ===\n", i + 1, daily->date);
        printf("Condition: %s\n", day->condition.text);
        printf("Temperature: %.1f°C to %.1f°C (%.1f°F to %.1f°F)\n", 
               day->mintemp_c, day->maxtemp_c, day->mintemp_f, day->maxtemp_f);
        printf("Average: %.1f°C (%.1f°F)\n", day->avgtemp_c, day->avgtemp_f);
        printf("Max Wind: %.1f kph (%.1f mph)\n", day->maxwind_kph, day->maxwind_mph);
        printf("Precipitation: %.1f mm (%.2f in)\n", day->totalprecip_mm, day->totalprecip_in);
        if (day->totalsnow_cm > 0) {
            printf("Snow: %.1f cm\n", day->totalsnow_cm);
        }
        printf("Humidity: %d%%\n", day->avghumidity);
        printf("UV Index: %.1f\n", day->uv);
        
        if (day->daily_chance_of_rain > 0) {
            printf("Chance of Rain: %d%%\n", day->daily_chance_of_rain);
        }
        if (day->daily_chance_of_snow > 0) {
            printf("Chance of Snow: %d%%\n", day->daily_chance_of_snow);
        }
        
        printf("Sunrise: %s, Sunset: %s\n", daily->astro.sunrise, daily->astro.sunset);
        printf("Moon: %s (%d%% illuminated)\n", daily->astro.moon_phase, daily->astro.moon_illumination);
        
        if (show_hourly && daily->hour_count > 0) {
            printf("\n--- Hourly Forecast ---\n");
            for (int h = 0; h < daily->hour_count; h++) {
                const forecast_hour_t *hour = &daily->hour[h];
                printf("%s: %.1f°C, %s, Rain: %d%%, Wind: %.1f kph\n",
                       hour->time, hour->temp_c, hour->condition.text,
                       hour->chance_of_rain, hour->wind_kph);
            }
        }
        
        printf("\n");
    }
}