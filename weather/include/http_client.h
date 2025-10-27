#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "weather_types.h"

/**
 * Initialize the HTTP client (must be called before using other functions)
 * @return 0 on success, -1 on error
 */
int http_client_init(void);

/**
 * Cleanup the HTTP client (should be called when done)
 */
void http_client_cleanup(void);

/**
 * Perform an HTTP GET request
 * @param url The URL to request
 * @param response Pointer to store the response (caller must free response->data)
 * @return 0 on success, -1 on error
 */
int http_get(const char *url, http_response_t *response);

/**
 * Perform an HTTP POST request with JSON data
 * @param url The URL to request
 * @param json_data The JSON data to send in the request body
 * @param response Pointer to store the response (caller must free response->data)
 * @return 0 on success, -1 on error
 */
int http_post_json(const char *url, const char *json_data, http_response_t *response);

/**
 * Free memory allocated for an HTTP response
 * @param response The response to free
 */
void http_response_free(http_response_t *response);

#endif // HTTP_CLIENT_H