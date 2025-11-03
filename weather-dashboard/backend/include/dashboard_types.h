#ifndef DASHBOARD_TYPES_H
#define DASHBOARD_TYPES_H

#include <stdbool.h>
#include <time.h>

#define MAX_NAME_LENGTH 256
#define MAX_LOCATION_LENGTH 256
#define MAX_URL_LENGTH 512
#define MAX_USER_ID_LENGTH 128
#define MAX_SESSION_ID_LENGTH 64
#define MAX_TOKEN_LENGTH 1024
#define MAX_USERS 100

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
    char user_id[MAX_USER_ID_LENGTH];  // Empty string = default/anonymous user
    char name[MAX_NAME_LENGTH];
    temperature_unit_t temp_unit;
    wind_unit_t wind_unit;
    char default_location[MAX_LOCATION_LENGTH];
    bool is_authenticated;
} user_profile_t;

// Session structure with OIDC tokens
typedef struct {
    char session_id[MAX_SESSION_ID_LENGTH];
    char user_id[MAX_USER_ID_LENGTH];
    time_t created_at;
    time_t last_accessed;
    time_t expires_at;  // Session expiry
    bool is_active;
    // OIDC tokens (empty if not using OIDC)
    char access_token[MAX_TOKEN_LENGTH];
    char refresh_token[MAX_TOKEN_LENGTH];
    char id_token[MAX_TOKEN_LENGTH];
    time_t token_expires_at;  // When access token expires
} user_session_t;

// Server configuration
typedef struct {
    int port;
    char bind_address[64];
    char static_path[256];
    char weather_service_url[MAX_URL_LENGTH];
    char database_url[MAX_URL_LENGTH];
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