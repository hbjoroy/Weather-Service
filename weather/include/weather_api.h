#ifndef WEATHER_API_H
#define WEATHER_API_H

#include "weather_types.h"

/**
 * Initialize the weather API client
 * @param config Configuration for the weather API
 * @return 0 on success, -1 on error
 */
int weather_api_init(const weather_config_t *config);

/**
 * Cleanup the weather API client
 */
void weather_api_cleanup(void);

/**
 * Get current weather for a location
 * @param location Location query (city name, coordinates, etc.)
 * @param include_aqi Whether to include air quality data (0 = no, 1 = yes)
 * @param response Pointer to store the parsed weather response
 * @return 0 on success, -1 on error
 */
int weather_api_get_current(const char *location, int include_aqi, weather_response_t *response);

/**
 * Get weather forecast for a location
 * @param location Location query (city name, coordinates, etc.)
 * @param days Number of forecast days (1-14)
 * @param include_aqi Whether to include air quality data (0 = no, 1 = yes)
 * @param include_alerts Whether to include weather alerts (0 = no, 1 = yes)
 * @param response Pointer to store the parsed forecast response
 * @return 0 on success, -1 on error
 */
int weather_api_get_forecast(const char *location, int days, int include_aqi, int include_alerts, forecast_response_t *response);

/**
 * Free memory allocated for a weather response
 * @param response The response to free
 */
void weather_response_free(weather_response_t *response);

/**
 * Free memory allocated for a forecast response
 * @param response The forecast response to free
 */
void forecast_response_free(forecast_response_t *response);

/**
 * Print weather information in a human-readable format
 * @param response The weather response to print
 */
void weather_print_current(const weather_response_t *response);

/**
 * Print forecast information in a human-readable format
 * @param response The forecast response to print
 * @param show_hourly Whether to show hourly details (0 = no, 1 = yes)
 */
void weather_print_forecast(const forecast_response_t *response, int show_hourly);

#endif // WEATHER_API_H