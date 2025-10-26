#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "http_server.h"
#include "weather_client.h"

static struct MHD_Daemon *daemon = NULL;
static server_config_t server_config;
static user_profile_t current_user;

// MIME type mapping
static const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    
    return "application/octet-stream";
}

// Add CORS headers
static void add_cors_headers(struct MHD_Response *response) {
    if (server_config.cors_enabled) {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
}

// Create JSON error response
static struct MHD_Response* create_error_response(int code, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(json, "error", error);
    
    char *json_string = cJSON_Print(json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    cJSON_Delete(json);
    return response;
}

// Handle profile GET request
static enum MHD_Result handle_profile_get(struct MHD_Connection *connection) {
    cJSON *json = cJSON_CreateObject();
    
    cJSON_AddStringToObject(json, "name", current_user.name);
    cJSON_AddStringToObject(json, "tempUnit", 
        current_user.temp_unit == TEMP_CELSIUS ? "celsius" : "fahrenheit");
    cJSON_AddStringToObject(json, "windUnit",
        current_user.wind_unit == WIND_KMH ? "kmh" : 
        current_user.wind_unit == WIND_KNOTS ? "knots" : "ms");
    cJSON_AddStringToObject(json, "defaultLocation", current_user.default_location);
    
    char *json_string = cJSON_Print(json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    cJSON_Delete(json);
    
    return ret;
}

// Handle profile PUT request
static enum MHD_Result handle_profile_put(struct MHD_Connection *connection,
                                         const char *upload_data, size_t *upload_data_size) {
    if (*upload_data_size == 0) {
        return MHD_YES; // Still receiving data
    }
    
    // Parse JSON request
    cJSON *json = cJSON_Parse(upload_data);
    if (!json) {
        struct MHD_Response *response = create_error_response(400, "Invalid JSON");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Update profile
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *temp_unit = cJSON_GetObjectItem(json, "tempUnit");
    cJSON *wind_unit = cJSON_GetObjectItem(json, "windUnit");
    cJSON *default_location = cJSON_GetObjectItem(json, "defaultLocation");
    
    if (name && cJSON_IsString(name)) {
        strncpy(current_user.name, name->valuestring, MAX_NAME_LENGTH - 1);
        current_user.name[MAX_NAME_LENGTH - 1] = '\0';
    }
    
    if (temp_unit && cJSON_IsString(temp_unit)) {
        if (strcmp(temp_unit->valuestring, "fahrenheit") == 0) {
            current_user.temp_unit = TEMP_FAHRENHEIT;
        } else {
            current_user.temp_unit = TEMP_CELSIUS;
        }
    }

    if (wind_unit && cJSON_IsString(wind_unit)) {
        if (strcmp(wind_unit->valuestring, "knots") == 0) {
            current_user.wind_unit = WIND_KNOTS;
        } else if (strcmp(wind_unit->valuestring, "ms") == 0) {
            current_user.wind_unit = WIND_MS;
        } else {
            current_user.wind_unit = WIND_KMH;
        }
    }
    
    if (default_location && cJSON_IsString(default_location)) {
        strncpy(current_user.default_location, default_location->valuestring, MAX_LOCATION_LENGTH - 1);
        current_user.default_location[MAX_LOCATION_LENGTH - 1] = '\0';
    }
    
    cJSON_Delete(json);
    *upload_data_size = 0; // Reset for next request
    
    // Return updated profile
    return handle_profile_get(connection);
}

// Handle weather current request
static enum MHD_Result handle_weather_current(struct MHD_Connection *connection) {
    const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
    const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
    
    if (!location) {
        struct MHD_Response *response = create_error_response(400, "Missing 'location' parameter");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    bool include_aqi = aqi_str && strcmp(aqi_str, "true") == 0;
    
    char *weather_response;
    int result = weather_client_get_current(location, include_aqi, &weather_response);
    
    if (result != 0) {
        struct MHD_Response *response = create_error_response(500, "Failed to fetch weather data");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(weather_response), weather_response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle weather forecast request
static enum MHD_Result handle_weather_forecast(struct MHD_Connection *connection) {
    const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
    const char *days_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "days");
    const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
    const char *alerts_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_alerts");
    const char *hourly_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_hourly");
    
    if (!location || !days_str) {
        struct MHD_Response *response = create_error_response(400, "Missing 'location' or 'days' parameter");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    weather_request_t request;
    strncpy(request.location, location, MAX_LOCATION_LENGTH - 1);
    request.location[MAX_LOCATION_LENGTH - 1] = '\0';
    request.days = atoi(days_str);
    request.include_aqi = aqi_str && strcmp(aqi_str, "true") == 0;
    request.include_alerts = alerts_str && strcmp(alerts_str, "true") == 0;
    request.include_hourly = hourly_str && strcmp(hourly_str, "true") == 0;
    
    if (request.days < 1 || request.days > 14) {
        struct MHD_Response *response = create_error_response(400, "Invalid days parameter (must be 1-14)");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    char *weather_response;
    int result = weather_client_get_forecast(&request, &weather_response);
    
    if (result != 0) {
        struct MHD_Response *response = create_error_response(500, "Failed to fetch forecast data");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(weather_response), weather_response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Serve static files
static enum MHD_Result serve_static_file(struct MHD_Connection *connection, const char *url) {
    char filepath[512];
    
    // Handle root path
    if (strcmp(url, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", server_config.static_path);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", server_config.static_path, url);
    }
    
    // Security check - prevent directory traversal
    if (strstr(filepath, "..") != NULL) {
        struct MHD_Response *response = create_error_response(403, "Access denied");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FORBIDDEN, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Open file
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        struct MHD_Response *response = create_error_response(404, "File not found");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Get file size
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
        struct MHD_Response *response = create_error_response(500, "File stat error");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_fd(file_stat.st_size, fd);
    if (!response) {
        close(fd);
        struct MHD_Response *error_response = create_error_response(500, "Failed to create response");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, error_response);
        MHD_destroy_response(error_response);
        return ret;
    }
    
    // Set content type
    const char *mime_type = get_mime_type(filepath);
    MHD_add_response_header(response, "Content-Type", mime_type);
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Main request handler
static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                          const char *url, const char *method,
                                          const char *version, const char *upload_data,
                                          size_t *upload_data_size, void **con_cls) {
    (void)cls; (void)version; (void)con_cls;
    
    if (server_config.verbose) {
        printf("Request: %s %s\n", method, url);
    }
    
    // Handle OPTIONS for CORS
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        add_cors_headers(response);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // API routes
    if (strncmp(url, "/api/", 5) == 0) {
        if (strcmp(url, "/api/profile") == 0) {
            if (strcmp(method, "GET") == 0) {
                return handle_profile_get(connection);
            } else if (strcmp(method, "PUT") == 0) {
                return handle_profile_put(connection, upload_data, upload_data_size);
            }
        } else if (strcmp(url, "/api/weather/current") == 0 && strcmp(method, "GET") == 0) {
            return handle_weather_current(connection);
        } else if (strcmp(url, "/api/weather/forecast") == 0 && strcmp(method, "GET") == 0) {
            return handle_weather_forecast(connection);
        }
        
        // API endpoint not found
        struct MHD_Response *response = create_error_response(404, "API endpoint not found");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Serve static files
    if (strcmp(method, "GET") == 0) {
        return serve_static_file(connection, url);
    }
    
    // Method not allowed
    struct MHD_Response *response = create_error_response(405, "Method not allowed");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
    MHD_destroy_response(response);
    return ret;
}

// Profile management functions
int profile_init(void) {
    // Initialize default user with UTF-8 Greek name
    strcpy(current_user.name, "Χαράλαμπους Μπιγγ");
    current_user.temp_unit = TEMP_CELSIUS;
    current_user.wind_unit = WIND_MS;
    strcpy(current_user.default_location, "Athens");
    
    if (server_config.verbose) {
        printf("Initialized default user profile: %s\n", current_user.name);
        printf("Temperature unit: %s\n", current_user.temp_unit == TEMP_CELSIUS ? "Celsius" : "Fahrenheit");
        printf("Wind unit: %s\n", 
            current_user.wind_unit == WIND_KMH ? "km/h" : 
            current_user.wind_unit == WIND_KNOTS ? "knots" : "m/s");
        printf("Default location: %s\n", current_user.default_location);
    }
    
    return 0;
}

user_profile_t* profile_get_current(void) {
    return &current_user;
}

int profile_update(const user_profile_t *profile) {
    if (!profile) return -1;
    
    memcpy(&current_user, profile, sizeof(user_profile_t));
    return 0;
}

void profile_cleanup(void) {
    // Nothing to cleanup for in-memory storage
}

// Server functions
int http_server_start(const server_config_t *config) {
    if (!config) {
        fprintf(stderr, "Invalid server configuration\n");
        return -1;
    }
    
    memcpy(&server_config, config, sizeof(server_config_t));
    
    // Initialize profile
    if (profile_init() != 0) {
        fprintf(stderr, "Failed to initialize user profile\n");
        return -1;
    }
    
    // Initialize weather client
    if (weather_client_init(config->weather_service_url) != 0) {
        fprintf(stderr, "Failed to initialize weather client\n");
        return -1;
    }
    
    // Start HTTP daemon
    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
                             config->port,
                             NULL, NULL,
                             &answer_to_connection, NULL,
                             MHD_OPTION_END);
    
    if (!daemon) {
        fprintf(stderr, "Failed to start HTTP server on port %d\n", config->port);
        weather_client_cleanup();
        return -1;
    }
    
    printf("Weather Dashboard Server started on %s:%d\n", config->bind_address, config->port);
    printf("Static files served from: %s\n", config->static_path);
    printf("Weather service URL: %s\n", config->weather_service_url);
    printf("CORS enabled: %s\n", config->cors_enabled ? "Yes" : "No");
    printf("Verbose logging: %s\n", config->verbose ? "Yes" : "No");
    
    return 0;
}

void http_server_stop(void) {
    if (daemon) {
        printf("Stopping server...\n");
        MHD_stop_daemon(daemon);
        daemon = NULL;
    }
    
    weather_client_cleanup();
    profile_cleanup();
    printf("Server stopped.\n");
}