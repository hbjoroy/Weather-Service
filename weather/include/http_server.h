#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "weather_types.h"

/**
 * Initialize the HTTP server
 * @param server_config Server configuration
 * @param weather_config Weather API configuration
 * @return 0 on success, -1 on error
 */
int http_server_init(const server_config_t *server_config, const weather_config_t *weather_config);

/**
 * Start the HTTP server (blocking call)
 * @return 0 on success, -1 on error
 */
int http_server_start(void);

/**
 * Stop the HTTP server
 */
void http_server_stop(void);

/**
 * Cleanup the HTTP server
 */
void http_server_cleanup(void);

/**
 * Set server to verbose mode for debugging
 * @param verbose 1 to enable verbose logging, 0 to disable
 */
void http_server_set_verbose(int verbose);

#endif // HTTP_SERVER_H