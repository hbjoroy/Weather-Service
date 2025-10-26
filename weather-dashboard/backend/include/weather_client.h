#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include "dashboard_types.h"

// Initialize weather client
int weather_client_init(const char *weather_service_url);

// Make requests to weather service
int weather_client_get_current(const char *location, bool include_aqi, char **response);
int weather_client_get_forecast(const weather_request_t *request, char **response);

// Cleanup
void weather_client_cleanup(void);

#endif // WEATHER_CLIENT_H