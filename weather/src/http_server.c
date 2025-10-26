#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <unistd.h>
#include "http_server.h"
#include "weather_api.h"

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