#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "weather_client.h"

static char weather_service_url[MAX_URL_LENGTH];
static int client_initialized = 0;

// HTTP response structure
typedef struct {
    char *data;
    size_t size;
} http_response_t;

// Callback function to write received data
static size_t write_callback(void *contents, size_t size, size_t nmemb, http_response_t *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0; // Null terminate
    
    return realsize;
}

// Make HTTP GET request
static int make_http_request(const char *url, char **response_data) {
    CURL *curl;
    CURLcode res;
    http_response_t response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return -1;
    }
    
    // Initialize response
    response.data = malloc(1);
    response.size = 0;
    
    if (!response.data) {
        fprintf(stderr, "Failed to allocate memory for response\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Set options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Weather-Dashboard/1.0");
    
    // Perform request
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Check HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);
    
    if (http_code != 200) {
        fprintf(stderr, "HTTP request failed with status: %ld\n", http_code);
        fprintf(stderr, "Response: %s\n", response.data);
        free(response.data);
        return -1;
    }
    
    *response_data = response.data;
    return 0;
}

int weather_client_init(const char *service_url) {
    if (!service_url) {
        fprintf(stderr, "Invalid weather service URL\n");
        return -1;
    }
    
    strncpy(weather_service_url, service_url, MAX_URL_LENGTH - 1);
    weather_service_url[MAX_URL_LENGTH - 1] = '\0';
    
    // Initialize curl globally
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
        return -1;
    }
    
    client_initialized = 1;
    printf("Weather client initialized with service URL: %s\n", weather_service_url);
    
    return 0;
}

int weather_client_get_current(const char *location, bool include_aqi, char **response) {
    if (!client_initialized) {
        fprintf(stderr, "Weather client not initialized\n");
        return -1;
    }
    
    if (!location || !response) {
        fprintf(stderr, "Invalid arguments to weather_client_get_current\n");
        return -1;
    }
    
    // URL encode location
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl for URL encoding\n");
        return -1;
    }
    
    char *encoded_location = curl_easy_escape(curl, location, 0);
    if (!encoded_location) {
        fprintf(stderr, "Failed to URL encode location\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Build URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/current?location=%s&include_aqi=%s",
             weather_service_url, encoded_location, include_aqi ? "true" : "false");
    
    curl_free(encoded_location);
    curl_easy_cleanup(curl);
    
    return make_http_request(url, response);
}

int weather_client_get_forecast(const weather_request_t *request, char **response) {
    if (!client_initialized) {
        fprintf(stderr, "Weather client not initialized\n");
        return -1;
    }
    
    if (!request || !response) {
        fprintf(stderr, "Invalid arguments to weather_client_get_forecast\n");
        return -1;
    }
    
    // URL encode location
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl for URL encoding\n");
        return -1;
    }
    
    char *encoded_location = curl_easy_escape(curl, request->location, 0);
    if (!encoded_location) {
        fprintf(stderr, "Failed to URL encode location\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Build URL
    char url[1024];
    snprintf(url, sizeof(url), "%s/forecast?location=%s&days=%d&include_aqi=%s&include_alerts=%s&include_hourly=%s",
             weather_service_url,
             encoded_location,
             request->days,
             request->include_aqi ? "true" : "false",
             request->include_alerts ? "true" : "false",
             request->include_hourly ? "true" : "false");
    
    curl_free(encoded_location);
    curl_easy_cleanup(curl);
    
    return make_http_request(url, response);
}

void weather_client_cleanup(void) {
    if (client_initialized) {
        curl_global_cleanup();
        client_initialized = 0;
        printf("Weather client cleaned up\n");
    }
}