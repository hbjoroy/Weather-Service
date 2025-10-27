#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <unistd.h>
#include <curl/curl.h>
#include "http_server.h"
#include "weather_api.h"
#include "http_client.h"

#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 65536

static struct MHD_Daemon *httpd = NULL;
static server_config_t server_cfg;
static weather_config_t weather_cfg;
static int server_verbose = 0;
static volatile int server_running = 1;

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int sig) {
    (void)sig;
    server_running = 0;
    printf("\nShutting down server...\n");
}

/**
 * Helper function to create JSON error response
 */
static cJSON* create_error_response(int code, const char* message, const char* details) {
    cJSON *error = cJSON_CreateObject();
    cJSON *error_obj = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(error_obj, "code", code);
    cJSON_AddStringToObject(error_obj, "message", message);
    if (details) {
        cJSON_AddStringToObject(error_obj, "details", details);
    }
    
    cJSON_AddItemToObject(error, "error", error_obj);
    return error;
}

/**
 * Helper function to create CORS headers
 */
static void add_cors_headers(struct MHD_Response *response) {
    if (server_cfg.enable_cors) {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
    }
}

/**
 * Send a message to a Slack channel
 */
static int send_slack_message(const char *channel, const char *text) {
    if (!server_cfg.slack_bot_token[0]) {
        fprintf(stderr, "Slack bot token not configured\n");
        return -1;
    }
    
    // Create JSON payload
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "channel", channel);
    cJSON_AddStringToObject(payload, "text", text);
    char *json_str = cJSON_Print(payload);
    cJSON_Delete(payload);
    
    if (!json_str) {
        fprintf(stderr, "Failed to create JSON payload\n");
        return -1;
    }
    
    // Prepare URL with authorization header
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", server_cfg.slack_bot_token);
    
    if (server_verbose) {
        printf("Sending Slack message to channel %s: %s\n", channel, text);
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        free(json_str);
        return -1;
    }
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/chat.postMessage");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_str);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to send Slack message: %s\n", curl_easy_strerror(res));
        return -1;
    }
    
    return 0;
}

/**
 * Check if a string contains "paros" (case insensitive)
 */
static int contains_paros(const char *text) {
    if (!text) return 0;
    
    // Create lowercase copy for case-insensitive search
    size_t len = strlen(text);
    char *lower = malloc(len + 1);
    if (!lower) return 0;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)text[i]);
    }
    lower[len] = '\0';
    
    int found = (strstr(lower, "paros") != NULL);
    free(lower);
    return found;
}

/**
 * Handle Slack message event - check for "paros" and respond with weather
 */
static void handle_paros_weather_request(const char *channel) {
    weather_response_t response;
    
    // Fetch weather for Paros, Greece
    if (weather_api_get_current("Paros, Greece", 0, &response) != 0) {
        if (server_verbose) {
            printf("Failed to fetch weather for Paros\n");
        }
        send_slack_message(channel, "Beklager, kunne ikkje hente vêrdata for Paros akkurat no.");
        return;
    }
    
    // Convert wind speed from km/h to m/s (1 km/h = 0.277778 m/s)
    double wind_ms = response.current.wind_kph * 0.277778;
    
    // Format the message in Norwegian
    char message[512];
    snprintf(message, sizeof(message),
             "På Paros er det no %.1f grader og %s, vinden er %.1f m/s, retning %s",
             response.current.temp_c,
             response.current.condition.text,
             wind_ms,
             response.current.wind_dir);
    
    if (server_verbose) {
        printf("Responding with Paros weather: %s\n", message);
    }
    
    send_slack_message(channel, message);
}

/**
 * Convert weather_response_t to JSON
 */
static cJSON* weather_response_to_json(const weather_response_t *response) {
    cJSON *json = cJSON_CreateObject();
    
    // Location
    cJSON *location = cJSON_CreateObject();
    cJSON_AddStringToObject(location, "name", response->location.name);
    cJSON_AddStringToObject(location, "region", response->location.region);
    cJSON_AddStringToObject(location, "country", response->location.country);
    cJSON_AddNumberToObject(location, "lat", response->location.lat);
    cJSON_AddNumberToObject(location, "lon", response->location.lon);
    cJSON_AddStringToObject(location, "tz_id", response->location.tz_id);
    cJSON_AddNumberToObject(location, "localtime_epoch", response->location.localtime_epoch);
    cJSON_AddStringToObject(location, "localtime", response->location.localtime);
    cJSON_AddItemToObject(json, "location", location);
    
    // Current weather
    cJSON *current = cJSON_CreateObject();
    cJSON_AddNumberToObject(current, "last_updated_epoch", response->current.last_updated_epoch);
    cJSON_AddStringToObject(current, "last_updated", response->current.last_updated);
    cJSON_AddNumberToObject(current, "temp_c", response->current.temp_c);
    cJSON_AddNumberToObject(current, "temp_f", response->current.temp_f);
    cJSON_AddNumberToObject(current, "is_day", response->current.is_day);
    
    // Condition
    cJSON *condition = cJSON_CreateObject();
    cJSON_AddStringToObject(condition, "text", response->current.condition.text);
    cJSON_AddStringToObject(condition, "icon", response->current.condition.icon);
    cJSON_AddNumberToObject(condition, "code", response->current.condition.code);
    cJSON_AddItemToObject(current, "condition", condition);
    
    cJSON_AddNumberToObject(current, "wind_mph", response->current.wind_mph);
    cJSON_AddNumberToObject(current, "wind_kph", response->current.wind_kph);
    cJSON_AddNumberToObject(current, "wind_degree", response->current.wind_degree);
    cJSON_AddStringToObject(current, "wind_dir", response->current.wind_dir);
    cJSON_AddNumberToObject(current, "pressure_mb", response->current.pressure_mb);
    cJSON_AddNumberToObject(current, "pressure_in", response->current.pressure_in);
    cJSON_AddNumberToObject(current, "precip_mm", response->current.precip_mm);
    cJSON_AddNumberToObject(current, "precip_in", response->current.precip_in);
    cJSON_AddNumberToObject(current, "humidity", response->current.humidity);
    cJSON_AddNumberToObject(current, "cloud", response->current.cloud);
    cJSON_AddNumberToObject(current, "feelslike_c", response->current.feelslike_c);
    cJSON_AddNumberToObject(current, "feelslike_f", response->current.feelslike_f);
    cJSON_AddNumberToObject(current, "vis_km", response->current.vis_km);
    cJSON_AddNumberToObject(current, "vis_miles", response->current.vis_miles);
    cJSON_AddNumberToObject(current, "uv", response->current.uv);
    cJSON_AddNumberToObject(current, "gust_mph", response->current.gust_mph);
    cJSON_AddNumberToObject(current, "gust_kph", response->current.gust_kph);
    
    cJSON_AddItemToObject(json, "current", current);
    return json;
}

/**
 * Convert forecast_response_t to JSON
 */
static cJSON* forecast_response_to_json(const forecast_response_t *response, int include_hourly) {
    cJSON *json = cJSON_CreateObject();
    
    // Location (reuse function logic)
    cJSON *location = cJSON_CreateObject();
    cJSON_AddStringToObject(location, "name", response->location.name);
    cJSON_AddStringToObject(location, "region", response->location.region);
    cJSON_AddStringToObject(location, "country", response->location.country);
    cJSON_AddNumberToObject(location, "lat", response->location.lat);
    cJSON_AddNumberToObject(location, "lon", response->location.lon);
    cJSON_AddStringToObject(location, "tz_id", response->location.tz_id);
    cJSON_AddNumberToObject(location, "localtime_epoch", response->location.localtime_epoch);
    cJSON_AddStringToObject(location, "localtime", response->location.localtime);
    cJSON_AddItemToObject(json, "location", location);
    
    // Forecast
    cJSON *forecast_obj = cJSON_CreateObject();
    cJSON *forecastday_array = cJSON_CreateArray();
    
    for (int i = 0; i < response->forecast_days; i++) {
        const forecast_daily_t *daily = &response->forecast[i];
        cJSON *day_obj = cJSON_CreateObject();
        
        cJSON_AddStringToObject(day_obj, "date", daily->date);
        cJSON_AddNumberToObject(day_obj, "date_epoch", daily->date_epoch);
        
        // Day data
        cJSON *day_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(day_data, "maxtemp_c", daily->day.maxtemp_c);
        cJSON_AddNumberToObject(day_data, "maxtemp_f", daily->day.maxtemp_f);
        cJSON_AddNumberToObject(day_data, "mintemp_c", daily->day.mintemp_c);
        cJSON_AddNumberToObject(day_data, "mintemp_f", daily->day.mintemp_f);
        cJSON_AddNumberToObject(day_data, "avgtemp_c", daily->day.avgtemp_c);
        cJSON_AddNumberToObject(day_data, "avgtemp_f", daily->day.avgtemp_f);
        cJSON_AddNumberToObject(day_data, "maxwind_mph", daily->day.maxwind_mph);
        cJSON_AddNumberToObject(day_data, "maxwind_kph", daily->day.maxwind_kph);
        cJSON_AddNumberToObject(day_data, "totalprecip_mm", daily->day.totalprecip_mm);
        cJSON_AddNumberToObject(day_data, "totalprecip_in", daily->day.totalprecip_in);
        cJSON_AddNumberToObject(day_data, "avghumidity", daily->day.avghumidity);
        cJSON_AddNumberToObject(day_data, "daily_will_it_rain", daily->day.daily_will_it_rain);
        cJSON_AddNumberToObject(day_data, "daily_chance_of_rain", daily->day.daily_chance_of_rain);
        cJSON_AddNumberToObject(day_data, "uv", daily->day.uv);
        
        // Day condition
        cJSON *day_condition = cJSON_CreateObject();
        cJSON_AddStringToObject(day_condition, "text", daily->day.condition.text);
        cJSON_AddStringToObject(day_condition, "icon", daily->day.condition.icon);
        cJSON_AddNumberToObject(day_condition, "code", daily->day.condition.code);
        cJSON_AddItemToObject(day_data, "condition", day_condition);
        
        cJSON_AddItemToObject(day_obj, "day", day_data);
        
        // Astronomy
        cJSON *astro = cJSON_CreateObject();
        cJSON_AddStringToObject(astro, "sunrise", daily->astro.sunrise);
        cJSON_AddStringToObject(astro, "sunset", daily->astro.sunset);
        cJSON_AddStringToObject(astro, "moonrise", daily->astro.moonrise);
        cJSON_AddStringToObject(astro, "moonset", daily->astro.moonset);
        cJSON_AddStringToObject(astro, "moon_phase", daily->astro.moon_phase);
        cJSON_AddNumberToObject(astro, "moon_illumination", daily->astro.moon_illumination);
        cJSON_AddItemToObject(day_obj, "astro", astro);
        
        // Hourly data (if requested)
        if (include_hourly && daily->hour_count > 0) {
            cJSON *hour_array = cJSON_CreateArray();
            for (int h = 0; h < daily->hour_count; h++) {
                const forecast_hour_t *hour = &daily->hour[h];
                cJSON *hour_obj = cJSON_CreateObject();
                
                cJSON_AddNumberToObject(hour_obj, "time_epoch", hour->time_epoch);
                cJSON_AddStringToObject(hour_obj, "time", hour->time);
                cJSON_AddNumberToObject(hour_obj, "temp_c", hour->temp_c);
                cJSON_AddNumberToObject(hour_obj, "temp_f", hour->temp_f);
                cJSON_AddNumberToObject(hour_obj, "is_day", hour->is_day);
                
                // Hour condition
                cJSON *hour_condition = cJSON_CreateObject();
                cJSON_AddStringToObject(hour_condition, "text", hour->condition.text);
                cJSON_AddStringToObject(hour_condition, "icon", hour->condition.icon);
                cJSON_AddNumberToObject(hour_condition, "code", hour->condition.code);
                cJSON_AddItemToObject(hour_obj, "condition", hour_condition);
                
                cJSON_AddNumberToObject(hour_obj, "wind_mph", hour->wind_mph);
                cJSON_AddNumberToObject(hour_obj, "wind_kph", hour->wind_kph);
                cJSON_AddNumberToObject(hour_obj, "wind_degree", hour->wind_degree);
                cJSON_AddStringToObject(hour_obj, "wind_dir", hour->wind_dir);
                cJSON_AddNumberToObject(hour_obj, "humidity", hour->humidity);
                cJSON_AddNumberToObject(hour_obj, "cloud", hour->cloud);
                cJSON_AddNumberToObject(hour_obj, "precip_mm", hour->precip_mm);
                cJSON_AddNumberToObject(hour_obj, "chance_of_rain", hour->chance_of_rain);
                
                cJSON_AddItemToArray(hour_array, hour_obj);
            }
            cJSON_AddItemToObject(day_obj, "hour", hour_array);
        }
        
        cJSON_AddItemToArray(forecastday_array, day_obj);
    }
    
    cJSON_AddItemToObject(forecast_obj, "forecastday", forecastday_array);
    cJSON_AddItemToObject(json, "forecast", forecast_obj);
    
    return json;
}

/**
 * Handle GET /current endpoint
 */
static enum MHD_Result handle_current_get(struct MHD_Connection *connection, const char *location, int include_aqi) {
    weather_response_t response;
    char *json_str;
    struct MHD_Response *http_response;
    enum MHD_Result ret;
    
    if (server_verbose) {
        printf("GET /current?location=%s&aqi=%d\n", location, include_aqi);
    }
    
    // Fetch weather data
    if (weather_api_get_current(location, include_aqi, &response) != 0) {
        cJSON *error = create_error_response(500, "Failed to fetch weather data", "Check if location exists and API is accessible");
        json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        http_response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(http_response, "Content-Type", "application/json");
        add_cors_headers(http_response);
        ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, http_response);
        MHD_destroy_response(http_response);
        return ret;
    }
    
    // Convert to JSON
    cJSON *json = weather_response_to_json(&response);
    json_str = cJSON_Print(json);
    cJSON_Delete(json);
    weather_response_free(&response);
    
    // Send response
    http_response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(http_response, "Content-Type", "application/json");
    add_cors_headers(http_response);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
    MHD_destroy_response(http_response);
    
    return ret;
}

/**
 * Handle POST /current endpoint
 */
static enum MHD_Result handle_current_post(struct MHD_Connection *connection, const char *upload_data, size_t *upload_data_size) {
    static char *post_data = NULL;
    static size_t post_data_len = 0;
    
    if (*upload_data_size != 0) {
        // Accumulate POST data
        post_data = realloc(post_data, post_data_len + *upload_data_size + 1);
        if (!post_data) {
            *upload_data_size = 0;
            return MHD_NO;
        }
        memcpy(post_data + post_data_len, upload_data, *upload_data_size);
        post_data_len += *upload_data_size;
        post_data[post_data_len] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // Process the complete POST data
    if (post_data == NULL) {
        cJSON *error = create_error_response(400, "No JSON data provided", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        add_cors_headers(response);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    if (server_verbose) {
        printf("POST /current with data: %s\n", post_data);
    }
    
    // Parse JSON request
    cJSON *json = cJSON_Parse(post_data);
    free(post_data);
    post_data = NULL;
    post_data_len = 0;
    
    if (!json) {
        cJSON *error = create_error_response(400, "Invalid JSON", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        add_cors_headers(response);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Extract parameters
    cJSON *location_json = cJSON_GetObjectItem(json, "location");
    cJSON *aqi_json = cJSON_GetObjectItem(json, "include_aqi");
    
    if (!location_json || !cJSON_IsString(location_json)) {
        cJSON_Delete(json);
        cJSON *error = create_error_response(400, "Missing or invalid 'location' field", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        add_cors_headers(response);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    const char *location = location_json->valuestring;
    int include_aqi = (aqi_json && cJSON_IsBool(aqi_json)) ? cJSON_IsTrue(aqi_json) : 0;
    
    cJSON_Delete(json);
    
    return handle_current_get(connection, location, include_aqi);
}

/**
 * Handle forecast endpoints (both GET and POST)
 */
static enum MHD_Result handle_forecast(struct MHD_Connection *connection, const char *location, int days, int include_aqi, int include_alerts, int include_hourly) {
    forecast_response_t response;
    char *json_str;
    struct MHD_Response *http_response;
    enum MHD_Result ret;
    
    if (server_verbose) {
        printf("Forecast request: location=%s, days=%d, aqi=%d, alerts=%d, hourly=%d\n", 
               location, days, include_aqi, include_alerts, include_hourly);
    }
    
    // Validate days
    if (days < 1 || days > 14) {
        cJSON *error = create_error_response(400, "Invalid days parameter", "Must be between 1 and 14");
        json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        http_response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(http_response, "Content-Type", "application/json");
        add_cors_headers(http_response);
        ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, http_response);
        MHD_destroy_response(http_response);
        return ret;
    }
    
    // Fetch forecast data
    if (weather_api_get_forecast(location, days, include_aqi, include_alerts, &response) != 0) {
        cJSON *error = create_error_response(500, "Failed to fetch forecast data", "Check if location exists and API is accessible");
        json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        http_response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(http_response, "Content-Type", "application/json");
        add_cors_headers(http_response);
        ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, http_response);
        MHD_destroy_response(http_response);
        return ret;
    }
    
    // Convert to JSON
    cJSON *json = forecast_response_to_json(&response, include_hourly);
    json_str = cJSON_Print(json);
    cJSON_Delete(json);
    forecast_response_free(&response);
    
    // Send response
    http_response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(http_response, "Content-Type", "application/json");
    add_cors_headers(http_response);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
    MHD_destroy_response(http_response);
    
    return ret;
}

/**
 * Handle health check endpoint
 */
static enum MHD_Result handle_health(struct MHD_Connection *connection) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "healthy");
    cJSON_AddStringToObject(json, "service", "weather-api");
    cJSON_AddStringToObject(json, "version", "1.0.0");
    
    char *json_str = cJSON_Print(json);
    cJSON_Delete(json);
    
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", "application/json");
    add_cors_headers(response);
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

/**
 * Handle Slack events endpoint
 */
static enum MHD_Result handle_slack_events(struct MHD_Connection *connection, 
                                          const char *upload_data,
                                          size_t *upload_data_size) {
    static char *post_data = NULL;
    static size_t post_data_size = 0;
    
    // Accumulate POST data
    if (*upload_data_size > 0) {
        char *new_data = realloc(post_data, post_data_size + *upload_data_size + 1);
        if (!new_data) {
            free(post_data);
            post_data = NULL;
            post_data_size = 0;
            
            cJSON *error = create_error_response(500, "Memory allocation failed", NULL);
            char *json_str = cJSON_Print(error);
            cJSON_Delete(error);
            
            struct MHD_Response *response = MHD_create_response_from_buffer(
                strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
            MHD_add_response_header(response, "Content-Type", "application/json");
            enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }
        
        post_data = new_data;
        memcpy(post_data + post_data_size, upload_data, *upload_data_size);
        post_data_size += *upload_data_size;
        post_data[post_data_size] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // Process the complete request
    if (post_data == NULL || post_data_size == 0) {
        cJSON *error = create_error_response(400, "Empty request body", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    if (server_verbose) {
        printf("POST /slack/events - Body: %s\n", post_data);
    }
    
    // Parse the JSON request
    cJSON *request = cJSON_Parse(post_data);
    if (!request) {
        free(post_data);
        post_data = NULL;
        post_data_size = 0;
        
        cJSON *error = create_error_response(400, "Invalid JSON", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Check for event type
    cJSON *type_item = cJSON_GetObjectItem(request, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        cJSON_Delete(request);
        free(post_data);
        post_data = NULL;
        post_data_size = 0;
        
        cJSON *error = create_error_response(400, "Missing 'type' field", NULL);
        char *json_str = cJSON_Print(error);
        cJSON_Delete(error);
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    const char *event_type = type_item->valuestring;
    struct MHD_Response *response;
    enum MHD_Result ret;
    char *json_str;
    
    // Handle URL verification challenge
    if (strcmp(event_type, "url_verification") == 0) {
        cJSON *challenge_item = cJSON_GetObjectItem(request, "challenge");
        if (!challenge_item || !cJSON_IsString(challenge_item)) {
            cJSON_Delete(request);
            free(post_data);
            post_data = NULL;
            post_data_size = 0;
            
            cJSON *error = create_error_response(400, "Missing 'challenge' field", NULL);
            json_str = cJSON_Print(error);
            cJSON_Delete(error);
            
            response = MHD_create_response_from_buffer(
                strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
            MHD_add_response_header(response, "Content-Type", "application/json");
            ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
            MHD_destroy_response(response);
            return ret;
        }
        
        const char *challenge = challenge_item->valuestring;
        
        if (server_verbose) {
            printf("Slack URL verification - challenge: %s\n", challenge);
        }
        
        // Create response with just the challenge string
        cJSON *response_json = cJSON_CreateObject();
        cJSON_AddStringToObject(response_json, "challenge", challenge);
        json_str = cJSON_Print(response_json);
        cJSON_Delete(response_json);
        
        response = MHD_create_response_from_buffer(
            strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        
        cJSON_Delete(request);
        free(post_data);
        post_data = NULL;
        post_data_size = 0;
        
        return ret;
    }
    
    // Handle event callbacks (messages, mentions, etc.)
    if (strcmp(event_type, "event_callback") == 0) {
        cJSON *event_item = cJSON_GetObjectItem(request, "event");
        if (event_item && cJSON_IsObject(event_item)) {
            cJSON *event_type_item = cJSON_GetObjectItem(event_item, "type");
            cJSON *text_item = cJSON_GetObjectItem(event_item, "text");
            cJSON *channel_item = cJSON_GetObjectItem(event_item, "channel");
            cJSON *subtype_item = cJSON_GetObjectItem(event_item, "subtype");
            cJSON *app_id_item = cJSON_GetObjectItem(event_item, "app_id");
            
            // Ignore bot messages to avoid infinite loops
            if (subtype_item && cJSON_IsString(subtype_item) && 
                strcmp(subtype_item->valuestring, "bot_message") == 0) {
                if (server_verbose) {
                    printf("Ignoring bot_message subtype to avoid loop\n");
                }
                
                cJSON *ack_response = cJSON_CreateObject();
                cJSON_AddStringToObject(ack_response, "status", "ok");
                json_str = cJSON_Print(ack_response);
                cJSON_Delete(ack_response);
                
                response = MHD_create_response_from_buffer(
                    strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
                MHD_add_response_header(response, "Content-Type", "application/json");
                ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
                MHD_destroy_response(response);
                
                cJSON_Delete(request);
                free(post_data);
                post_data = NULL;
                post_data_size = 0;
                
                return ret;
            }
            
            // Ignore messages from our own app_id
            if (app_id_item && cJSON_IsString(app_id_item) && 
                server_cfg.slack_app_id[0] != '\0') {
                if (strcmp(app_id_item->valuestring, server_cfg.slack_app_id) == 0) {
                    if (server_verbose) {
                        printf("Ignoring message from our own app_id: %s\n", app_id_item->valuestring);
                    }
                    
                    cJSON *ack_response = cJSON_CreateObject();
                    cJSON_AddStringToObject(ack_response, "status", "ok");
                    json_str = cJSON_Print(ack_response);
                    cJSON_Delete(ack_response);
                    
                    response = MHD_create_response_from_buffer(
                        strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
                    MHD_add_response_header(response, "Content-Type", "application/json");
                    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
                    MHD_destroy_response(response);
                    
                    cJSON_Delete(request);
                    free(post_data);
                    post_data = NULL;
                    post_data_size = 0;
                    
                    return ret;
                }
            }
            
            if (event_type_item && cJSON_IsString(event_type_item) &&
                text_item && cJSON_IsString(text_item) &&
                channel_item && cJSON_IsString(channel_item)) {
                
                const char *event_subtype = event_type_item->valuestring;
                const char *message_text = text_item->valuestring;
                const char *channel = channel_item->valuestring;
                
                if (server_verbose) {
                    printf("Received %s event in channel %s: %s\n", 
                           event_subtype, channel, message_text);
                }
                
                // Check if message contains "paros"
                if (contains_paros(message_text)) {
                    if (server_verbose) {
                        printf("Message contains 'paros' - fetching weather\n");
                    }
                    
                    // Handle this in a non-blocking way (acknowledge first, then respond)
                    // For now, we'll do it synchronously but Slack expects quick response
                    handle_paros_weather_request(channel);
                }
            }
        }
        
        // Acknowledge the event
        cJSON *ack_response = cJSON_CreateObject();
        cJSON_AddStringToObject(ack_response, "status", "ok");
        json_str = cJSON_Print(ack_response);
        cJSON_Delete(ack_response);
        
        response = MHD_create_response_from_buffer(
            strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        
        cJSON_Delete(request);
        free(post_data);
        post_data = NULL;
        post_data_size = 0;
        
        return ret;
    }
    
    // Handle other Slack events (to be implemented)
    if (server_verbose) {
        printf("Received Slack event type: %s\n", event_type);
    }
    
    // For now, acknowledge other events
    cJSON *ack_response = cJSON_CreateObject();
    cJSON_AddStringToObject(ack_response, "status", "ok");
    json_str = cJSON_Print(ack_response);
    cJSON_Delete(ack_response);
    
    response = MHD_create_response_from_buffer(
        strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", "application/json");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    cJSON_Delete(request);
    free(post_data);
    post_data = NULL;
    post_data_size = 0;
    
    return ret;
}

/**
 * Main HTTP request handler
 */
static enum MHD_Result request_handler(void *cls, struct MHD_Connection *connection,
                                      const char *url, const char *method,
                                      const char *version, const char *upload_data,
                                      size_t *upload_data_size, void **con_cls) {
    (void)cls;
    (void)version;
    static int aptr;
    
    if (*con_cls == NULL) {
        *con_cls = &aptr;
        return MHD_YES;
    }
    
    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        add_cors_headers(response);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Health check endpoint
    if (strcmp(url, "/health") == 0 && strcmp(method, "GET") == 0) {
        return handle_health(connection);
    }
    
    // Slack events endpoint
    if (strcmp(url, "/slack/events") == 0 && strcmp(method, "POST") == 0) {
        return handle_slack_events(connection, upload_data, upload_data_size);
    }
    
    // Current weather endpoints
    if (strcmp(url, "/current") == 0) {
        if (strcmp(method, "GET") == 0) {
            const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
            const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
            
            if (!location) {
                cJSON *error = create_error_response(400, "Missing 'location' parameter", NULL);
                char *json_str = cJSON_Print(error);
                cJSON_Delete(error);
                
                struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
                MHD_add_response_header(response, "Content-Type", "application/json");
                add_cors_headers(response);
                enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
                MHD_destroy_response(response);
                return ret;
            }
            
            int include_aqi = (aqi_str && (strcmp(aqi_str, "true") == 0 || strcmp(aqi_str, "1") == 0)) ? 1 : 0;
            return handle_current_get(connection, location, include_aqi);
        } else if (strcmp(method, "POST") == 0) {
            return handle_current_post(connection, upload_data, upload_data_size);
        }
    }
    
    // Forecast endpoints
    if (strncmp(url, "/forecast", 9) == 0) {
        if (strcmp(method, "GET") == 0) {
            const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
            const char *days_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "days");
            const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
            const char *alerts_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_alerts");
            const char *hourly_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_hourly");
            
            if (!location || !days_str) {
                cJSON *error = create_error_response(400, "Missing 'location' or 'days' parameter", NULL);
                char *json_str = cJSON_Print(error);
                cJSON_Delete(error);
                
                struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
                MHD_add_response_header(response, "Content-Type", "application/json");
                add_cors_headers(response);
                enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
                MHD_destroy_response(response);
                return ret;
            }
            
            int days = atoi(days_str);
            int include_aqi = (aqi_str && (strcmp(aqi_str, "true") == 0 || strcmp(aqi_str, "1") == 0)) ? 1 : 0;
            int include_alerts = (alerts_str && (strcmp(alerts_str, "true") == 0 || strcmp(alerts_str, "1") == 0)) ? 1 : 0;
            int include_hourly = (hourly_str && (strcmp(hourly_str, "true") == 0 || strcmp(hourly_str, "1") == 0)) ? 1 : 0;
            
            return handle_forecast(connection, location, days, include_aqi, include_alerts, include_hourly);
        }
        // POST forecast handling would go here if needed
    }
    
    // 404 Not Found
    cJSON *error = create_error_response(404, "Endpoint not found", NULL);
    char *json_str = cJSON_Print(error);
    cJSON_Delete(error);
    
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_str), json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", "application/json");
    add_cors_headers(response);
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    
    return ret;
}

int http_server_init(const server_config_t *server_config, const weather_config_t *weather_config) {
    if (!server_config || !weather_config) {
        fprintf(stderr, "Invalid configuration provided to http_server_init\n");
        return -1;
    }
    
    // Copy configurations
    memcpy(&server_cfg, server_config, sizeof(server_config_t));
    memcpy(&weather_cfg, weather_config, sizeof(weather_config_t));
    
    // Initialize weather API
    if (weather_api_init(weather_config) != 0) {
        fprintf(stderr, "Failed to initialize weather API\n");
        return -1;
    }
    
    return 0;
}

int http_server_start(void) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Starting HTTP server on %s:%d\n", 
           strlen(server_cfg.bind_address) > 0 ? server_cfg.bind_address : "0.0.0.0", 
           server_cfg.port);
    
    // Start HTTP daemon
    httpd = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        server_cfg.port,
        NULL, NULL,
        &request_handler, NULL,
        MHD_OPTION_CONNECTION_LIMIT, server_cfg.max_connections,
        MHD_OPTION_END
    );
    
    if (httpd == NULL) {
        fprintf(stderr, "Failed to start HTTP server\n");
        return -1;
    }
    
    printf("Weather API server running at http://%s:%d\n", 
           strlen(server_cfg.bind_address) > 0 ? server_cfg.bind_address : "localhost", 
           server_cfg.port);
    printf("Available endpoints:\n");
    printf("  GET  /health\n");
    printf("  POST /slack/events (Slack events webhook)\n");
    printf("  GET  /current?location=<location>&include_aqi=<true|false>\n");
    printf("  POST /current (JSON body)\n");
    printf("  GET  /forecast?location=<location>&days=<1-14>&include_aqi=<true|false>&include_alerts=<true|false>&include_hourly=<true|false>\n");
    printf("Press Ctrl+C to stop the server\n\n");
    
    // Server loop
    while (server_running) {
        sleep(1);
    }
    
    return 0;
}

void http_server_stop(void) {
    server_running = 0;
    if (httpd) {
        MHD_stop_daemon(httpd);
        httpd = NULL;
    }
}

void http_server_cleanup(void) {
    http_server_stop();
    weather_api_cleanup();
}

void http_server_set_verbose(int verbose) {
    server_verbose = verbose;
}