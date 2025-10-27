#ifndef WEATHER_TYPES_H
#define WEATHER_TYPES_H

/**
 * Weather condition structure
 */
typedef struct {
    char text[128];         // Weather condition text (e.g., "Sunny")
    char icon[256];         // Icon URL
    int code;               // Weather condition code
} weather_condition_t;

/**
 * Astronomy data for a day
 */
typedef struct {
    char sunrise[32];       // Sunrise time
    char sunset[32];        // Sunset time
    char moonrise[32];      // Moonrise time
    char moonset[32];       // Moonset time
    char moon_phase[64];    // Moon phase description
    int moon_illumination;  // Moon illumination percentage
} astronomy_t;

/**
 * Daily forecast day summary
 */
typedef struct {
    double maxtemp_c;       // Maximum temperature in Celsius
    double maxtemp_f;       // Maximum temperature in Fahrenheit
    double mintemp_c;       // Minimum temperature in Celsius
    double mintemp_f;       // Minimum temperature in Fahrenheit
    double avgtemp_c;       // Average temperature in Celsius
    double avgtemp_f;       // Average temperature in Fahrenheit
    double maxwind_mph;     // Maximum wind speed in mph
    double maxwind_kph;     // Maximum wind speed in kph
    double totalprecip_mm;  // Total precipitation in mm
    double totalprecip_in;  // Total precipitation in inches
    double totalsnow_cm;    // Total snow in cm
    double avgvis_km;       // Average visibility in km
    double avgvis_miles;    // Average visibility in miles
    int avghumidity;        // Average humidity percentage
    int daily_will_it_rain; // Will it rain (1 = yes, 0 = no)
    int daily_chance_of_rain; // Chance of rain percentage
    int daily_will_it_snow; // Will it snow (1 = yes, 0 = no)  
    int daily_chance_of_snow; // Chance of snow percentage
    weather_condition_t condition; // Weather condition
    double uv;              // UV index
} forecast_day_t;

/**
 * Hourly forecast data
 */
typedef struct {
    long time_epoch;        // Time as epoch
    char time[32];          // Time string
    double temp_c;          // Temperature in Celsius
    double temp_f;          // Temperature in Fahrenheit
    int is_day;             // Day/night indicator
    weather_condition_t condition; // Weather condition
    double wind_mph;        // Wind speed in mph
    double wind_kph;        // Wind speed in kph
    int wind_degree;        // Wind direction in degrees
    char wind_dir[8];       // Wind direction
    double pressure_mb;     // Pressure in millibars
    double pressure_in;     // Pressure in inches
    double precip_mm;       // Precipitation in mm
    double precip_in;       // Precipitation in inches
    int humidity;           // Humidity percentage
    int cloud;              // Cloud cover percentage
    double feelslike_c;     // Feels like temperature in Celsius
    double feelslike_f;     // Feels like temperature in Fahrenheit
    double windchill_c;     // Wind chill in Celsius
    double windchill_f;     // Wind chill in Fahrenheit
    double heatindex_c;     // Heat index in Celsius
    double heatindex_f;     // Heat index in Fahrenheit
    double dewpoint_c;      // Dew point in Celsius
    double dewpoint_f;      // Dew point in Fahrenheit
    int will_it_rain;       // Will it rain (1 = yes, 0 = no)
    int chance_of_rain;     // Chance of rain percentage
    int will_it_snow;       // Will it snow (1 = yes, 0 = no)
    int chance_of_snow;     // Chance of snow percentage
    double vis_km;          // Visibility in km
    double vis_miles;       // Visibility in miles
    double gust_mph;        // Wind gust in mph
    double gust_kph;        // Wind gust in kph
    double uv;              // UV index
} forecast_hour_t;

/**
 * Daily forecast structure
 */
typedef struct {
    char date[32];          // Date string (YYYY-MM-DD)
    long date_epoch;        // Date as epoch
    forecast_day_t day;     // Day summary
    astronomy_t astro;      // Astronomy data
    forecast_hour_t *hour;  // Array of hourly forecasts (24 hours)
    int hour_count;         // Number of hours (should be 24)
} forecast_daily_t;

/**
 * Location information structure
 */
typedef struct {
    char name[128];         // Location name
    char region[128];       // Region/state
    char country[128];      // Country
    double lat;             // Latitude
    double lon;             // Longitude
    char tz_id[64];         // Timezone ID
    long localtime_epoch;   // Local time as epoch
    char localtime[32];     // Local time string
} location_t;

/**
 * Current weather data structure
 */
typedef struct {
    long last_updated_epoch;    // Last update time as epoch
    char last_updated[32];      // Last update time string
    double temp_c;              // Temperature in Celsius
    double temp_f;              // Temperature in Fahrenheit
    int is_day;                 // Day/night indicator (1 = day, 0 = night)
    weather_condition_t condition; // Weather condition
    double wind_mph;            // Wind speed in mph
    double wind_kph;            // Wind speed in kph
    int wind_degree;            // Wind direction in degrees
    char wind_dir[8];           // Wind direction (N, NE, E, etc.)
    double pressure_mb;         // Pressure in millibars
    double pressure_in;         // Pressure in inches
    double precip_mm;           // Precipitation in mm
    double precip_in;           // Precipitation in inches
    int humidity;               // Humidity percentage
    int cloud;                  // Cloud cover percentage
    double feelslike_c;         // Feels like temperature in Celsius
    double feelslike_f;         // Feels like temperature in Fahrenheit
    double windchill_c;         // Wind chill in Celsius
    double windchill_f;         // Wind chill in Fahrenheit
    double heatindex_c;         // Heat index in Celsius
    double heatindex_f;         // Heat index in Fahrenheit
    double dewpoint_c;          // Dew point in Celsius
    double dewpoint_f;          // Dew point in Fahrenheit
    double vis_km;              // Visibility in km
    double vis_miles;           // Visibility in miles
    double uv;                  // UV index
    double gust_mph;            // Wind gust in mph
    double gust_kph;            // Wind gust in kph
    double short_rad;           // Short wave radiation
    double diff_rad;            // Diffuse radiation
    double dni;                 // Direct normal irradiance
    double gti;                 // Global tilted irradiance
} current_weather_t;

/**
 * Complete weather response structure
 */
typedef struct {
    location_t location;        // Location information
    current_weather_t current;  // Current weather data
} weather_response_t;

/**
 * Forecast response structure
 */
typedef struct {
    location_t location;        // Location information
    current_weather_t current;  // Current weather data (also included in forecast)
    forecast_daily_t *forecast; // Array of daily forecasts
    int forecast_days;          // Number of forecast days
} forecast_response_t;

/**
 * HTTP response structure
 */
typedef struct {
    char *data;                 // Response data
    size_t size;                // Size of response data
    long status_code;           // HTTP status code
} http_response_t;

/**
 * Weather API configuration
 */
typedef struct {
    char api_key[256];          // API key
    char base_url[512];         // Base URL for API
    int timeout;                // Request timeout in seconds
} weather_config_t;

/**
 * HTTP server configuration
 */
typedef struct {
    int port;                   // Server port
    int max_connections;        // Maximum concurrent connections
    char bind_address[64];      // IP address to bind to
    int enable_cors;            // Enable CORS headers
    char slack_bot_token[256];  // Slack Bot OAuth Token
    char slack_app_id[64];      // Slack App ID (to ignore own messages)
} server_config_t;

/**
 * JSON API request structure for current weather
 */
typedef struct {
    char location[256];         // Location query
    int include_aqi;            // Include air quality data
} current_request_t;

/**
 * JSON API request structure for forecast
 */
typedef struct {
    char location[256];         // Location query
    int days;                   // Number of forecast days (1-14)
    int include_aqi;            // Include air quality data
    int include_alerts;         // Include weather alerts
    int include_hourly;         // Include hourly forecast details
} forecast_request_t;

/**
 * API error response structure
 */
typedef struct {
    int code;                   // Error code
    char message[512];          // Error message
    char details[1024];         // Additional error details
} api_error_t;

#endif // WEATHER_TYPES_H