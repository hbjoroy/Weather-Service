#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "http_client.h"

static int curl_initialized = 0;

/**
 * Callback function to write received data into our response structure
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, http_response_t *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    
    if (ptr == NULL) {
        // Out of memory
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0; // Null terminate
    
    return realsize;
}

int http_client_init(void) {
    if (curl_initialized) {
        return 0; // Already initialized
    }
    
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
        return -1;
    }
    
    curl_initialized = 1;
    return 0;
}

void http_client_cleanup(void) {
    if (curl_initialized) {
        curl_global_cleanup();
        curl_initialized = 0;
    }
}

int http_get(const char *url, http_response_t *response) {
    if (!curl_initialized) {
        fprintf(stderr, "HTTP client not initialized. Call http_client_init() first.\n");
        return -1;
    }
    
    if (!url || !response) {
        fprintf(stderr, "Invalid arguments to http_get\n");
        return -1;
    }
    
    // Initialize response
    response->data = malloc(1);
    response->size = 0;
    response->status_code = 0;
    
    if (!response->data) {
        fprintf(stderr, "Failed to allocate memory for response\n");
        return -1;
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl handle\n");
        free(response->data);
        return -1;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url);
    
    // Set callback function to write received data
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set timeout (30 seconds)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Weather-Service/1.0");
    
    // Verify SSL certificates
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(response->data);
        response->data = NULL;
        return -1;
    }
    
    // Get HTTP status code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);
    
    curl_easy_cleanup(curl);
    return 0;
}

void http_response_free(http_response_t *response) {
    if (response && response->data) {
        free(response->data);
        response->data = NULL;
        response->size = 0;
        response->status_code = 0;
    }
}