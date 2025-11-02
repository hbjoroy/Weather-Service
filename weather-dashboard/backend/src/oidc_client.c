#define _GNU_SOURCE  // For asprintf
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "oidc_client.h"

static oidc_config_t config = {0};
static bool initialized = false;

// Helper for URL encoding
static char* url_encode(const char *str) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    char *encoded = curl_easy_escape(curl, str, 0);
    char *result = encoded ? strdup(encoded) : NULL;
    
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

// HTTP response buffer
struct response_buffer {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct response_buffer *mem = (struct response_buffer *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

// Discover OIDC endpoints from .well-known/openid-configuration
static bool discover_endpoints(const char *issuer) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    
    struct response_buffer response = {0};
    
    // Construct discovery URL
    char *discovery_url;
    char *issuer_copy = strdup(issuer);
    
    // Remove trailing slash if present
    if (issuer_copy[strlen(issuer_copy) - 1] == '/') {
        issuer_copy[strlen(issuer_copy) - 1] = '\0';
    }
    
    asprintf(&discovery_url, "%s/.well-known/openid-configuration", issuer_copy);
    free(issuer_copy);
    
    printf("Discovering OIDC configuration from: %s\n", discovery_url);
    
    curl_easy_setopt(curl, CURLOPT_URL, discovery_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    free(discovery_url);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to fetch discovery document: %s\n", curl_easy_strerror(res));
        free(response.data);
        return false;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response.data);
    free(response.data);
    
    if (!json) {
        fprintf(stderr, "Failed to parse discovery document\n");
        return false;
    }
    
    // Extract endpoints
    cJSON *item;
    
    item = cJSON_GetObjectItem(json, "authorization_endpoint");
    if (item && cJSON_IsString(item)) {
        config.authorization_endpoint = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "token_endpoint");
    if (item && cJSON_IsString(item)) {
        config.token_endpoint = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "userinfo_endpoint");
    if (item && cJSON_IsString(item)) {
        config.userinfo_endpoint = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "end_session_endpoint");
    if (item && cJSON_IsString(item)) {
        config.end_session_endpoint = strdup(item->valuestring);
    }
    
    cJSON_Delete(json);
    
    // Validate that we got the required endpoints
    if (!config.authorization_endpoint || !config.token_endpoint || !config.userinfo_endpoint) {
        fprintf(stderr, "Discovery document missing required endpoints\n");
        return false;
    }
    
    return true;
}

bool oidc_init(const char *issuer, const char *client_id, const char *client_secret, const char *redirect_uri) {
    if (initialized) {
        oidc_cleanup();
    }
    
    config.issuer = strdup(issuer);
    config.client_id = strdup(client_id);
    config.client_secret = strdup(client_secret);
    config.redirect_uri = strdup(redirect_uri);
    
    // Discover endpoints from .well-known/openid-configuration
    if (!discover_endpoints(issuer)) {
        fprintf(stderr, "Failed to discover OIDC endpoints, using fallback URLs\n");
        
        // Fallback: Construct standard OIDC endpoints
        char *base_url = strdup(issuer);
        if (base_url[strlen(base_url) - 1] == '/') {
            base_url[strlen(base_url) - 1] = '\0';
        }
        
        asprintf(&config.authorization_endpoint, "%s/authorize", base_url);
        asprintf(&config.token_endpoint, "%s/token", base_url);
        asprintf(&config.userinfo_endpoint, "%s/userinfo", base_url);
        asprintf(&config.end_session_endpoint, "%s/end-session", base_url);
        
        free(base_url);
    }
    
    initialized = true;
    printf("OIDC initialized:\n");
    printf("  Issuer: %s\n", config.issuer);
    printf("  Client ID: %s\n", config.client_id);
    printf("  Redirect URI: %s\n", config.redirect_uri);
    printf("  Authorization endpoint: %s\n", config.authorization_endpoint);
    printf("  Token endpoint: %s\n", config.token_endpoint);
    printf("  Userinfo endpoint: %s\n", config.userinfo_endpoint);
    if (config.end_session_endpoint) {
        printf("  End session endpoint: %s\n", config.end_session_endpoint);
    }
    
    return true;
}

bool oidc_is_configured(void) {
    return initialized;
}

char* oidc_get_authorization_url(const char *state) {
    if (!initialized) return NULL;
    
    char *encoded_redirect = url_encode(config.redirect_uri);
    char *encoded_state = url_encode(state);
    
    char *url;
    asprintf(&url, "%s?client_id=%s&redirect_uri=%s&response_type=code&scope=openid%%20profile%%20email&state=%s",
             config.authorization_endpoint,
             config.client_id,
             encoded_redirect,
             encoded_state);
    
    free(encoded_redirect);
    free(encoded_state);
    
    return url;
}

oidc_tokens_t* oidc_exchange_code(const char *code) {
    if (!initialized) return NULL;
    
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    struct response_buffer response = {0};
    
    // Prepare POST data
    char *post_data;
    char *encoded_redirect = url_encode(config.redirect_uri);
    char *encoded_code = url_encode(code);
    
    asprintf(&post_data, "grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&client_secret=%s",
             encoded_code, encoded_redirect, config.client_id, config.client_secret);
    
    free(encoded_redirect);
    free(encoded_code);
    
    curl_easy_setopt(curl, CURLOPT_URL, config.token_endpoint);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    free(post_data);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(response.data);
        return NULL;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response.data);
    free(response.data);
    
    if (!json) return NULL;
    
    oidc_tokens_t *tokens = calloc(1, sizeof(oidc_tokens_t));
    
    cJSON *item = cJSON_GetObjectItem(json, "access_token");
    if (item && cJSON_IsString(item)) {
        tokens->access_token = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "id_token");
    if (item && cJSON_IsString(item)) {
        tokens->id_token = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "refresh_token");
    if (item && cJSON_IsString(item)) {
        tokens->refresh_token = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "expires_in");
    if (item && cJSON_IsNumber(item)) {
        tokens->expires_in = item->valueint;
    }
    
    item = cJSON_GetObjectItem(json, "token_type");
    if (item && cJSON_IsString(item)) {
        tokens->token_type = strdup(item->valuestring);
    }
    
    cJSON_Delete(json);
    
    if (!tokens->access_token) {
        oidc_free_tokens(tokens);
        return NULL;
    }
    
    return tokens;
}

oidc_userinfo_t* oidc_get_userinfo(const char *access_token) {
    if (!initialized) return NULL;
    
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    struct response_buffer response = {0};
    
    char *auth_header;
    asprintf(&auth_header, "Authorization: Bearer %s", access_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_URL, config.userinfo_endpoint);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    free(auth_header);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(response.data);
        return NULL;
    }
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response.data);
    free(response.data);
    
    if (!json) return NULL;
    
    oidc_userinfo_t *userinfo = calloc(1, sizeof(oidc_userinfo_t));
    
    cJSON *item = cJSON_GetObjectItem(json, "sub");
    if (item && cJSON_IsString(item)) {
        userinfo->sub = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "name");
    if (item && cJSON_IsString(item)) {
        userinfo->name = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "email");
    if (item && cJSON_IsString(item)) {
        userinfo->email = strdup(item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json, "preferred_username");
    if (item && cJSON_IsString(item)) {
        userinfo->preferred_username = strdup(item->valuestring);
    }
    
    cJSON_Delete(json);
    
    if (!userinfo->sub) {
        oidc_free_userinfo(userinfo);
        return NULL;
    }
    
    return userinfo;
}

char* oidc_get_logout_url(const char *id_token_hint, const char *post_logout_redirect_uri) {
    if (!initialized) return NULL;
    
    char *url;
    if (id_token_hint && post_logout_redirect_uri) {
        char *encoded_redirect = url_encode(post_logout_redirect_uri);
        asprintf(&url, "%s?id_token_hint=%s&post_logout_redirect_uri=%s",
                 config.end_session_endpoint, id_token_hint, encoded_redirect);
        free(encoded_redirect);
    } else {
        url = strdup(config.end_session_endpoint);
    }
    
    return url;
}

void oidc_free_tokens(oidc_tokens_t *tokens) {
    if (!tokens) return;
    
    free(tokens->access_token);
    free(tokens->id_token);
    free(tokens->refresh_token);
    free(tokens->token_type);
    free(tokens);
}

void oidc_free_userinfo(oidc_userinfo_t *userinfo) {
    if (!userinfo) return;
    
    free(userinfo->sub);
    free(userinfo->name);
    free(userinfo->email);
    free(userinfo->preferred_username);
    free(userinfo);
}

void oidc_cleanup(void) {
    if (!initialized) return;
    
    free(config.issuer);
    free(config.client_id);
    free(config.client_secret);
    free(config.redirect_uri);
    free(config.authorization_endpoint);
    free(config.token_endpoint);
    free(config.userinfo_endpoint);
    free(config.end_session_endpoint);
    
    memset(&config, 0, sizeof(config));
    initialized = false;
}
