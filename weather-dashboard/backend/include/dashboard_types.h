#ifndef DASHBOARD_TYPES_H
#define DASHBOARD_TYPES_H

#include <stdbool.h>

#define MAX_NAME_LENGTH 256
#define MAX_LOCATION_LENGTH 256
#define MAX_URL_LENGTH 512

// Temperature unit preferences
typedef enum {
    TEMP_CELSIUS,
    TEMP_FAHRENHEIT
} temperature_unit_t;

// Wind speed unit preferences
typedef enum {
    WIND_KMH,
    WIND_KNOTS,
    WIND_MS
} wind_unit_t;

// User profile structure
typedef struct {
    char name[MAX_NAME_LENGTH];
    temperature_unit_t temp_unit;
    wind_unit_t wind_unit;
    char default_location[MAX_LOCATION_LENGTH];
} user_profile_t;

// Server configuration
typedef struct {
    int port;
    char bind_address[64];
    char static_path[256];
    char weather_service_url[MAX_URL_LENGTH];
    bool cors_enabled;
    bool verbose;
} server_config_t;

// Weather API request
typedef struct {
    char location[MAX_LOCATION_LENGTH];
    int days;
    bool include_aqi;
    bool include_alerts;
    bool include_hourly;
} weather_request_t;

#endif // DASHBOARD_TYPES_H