#define _GNU_SOURCE  // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "http_server.h"
#include "weather_client.h"
#include "session_manager.h"
#include "db_manager.h"
#include "oidc_client.h"

static struct MHD_Daemon *http_daemon = NULL;
static server_config_t server_config;

// Rate limiting configuration
#define MAX_TRACKED_IPS 1000
#define RATE_LIMIT_WINDOW 60  // 60 seconds
#define MAX_REQUESTS_PER_WINDOW 30  // 30 requests per minute

typedef struct {
    char ip_address[46];  // IPv6 max length
    time_t window_start;
    int request_count;
    int used;
} rate_limit_entry_t;

static rate_limit_entry_t rate_limits[MAX_TRACKED_IPS] = {0};

// Store pending OIDC states with PKCE verifiers (simple in-memory for now)
#define MAX_PENDING_STATES 100
typedef struct {
    char state[65];
    char code_verifier[129];  // PKCE code_verifier (128 chars + null)
    time_t created;
    int used;
} pending_state_t;
static pending_state_t pending_states[MAX_PENDING_STATES] = {0};

// Request context for POST data accumulation
struct request_context {
    char *data;
    size_t size;
};

// Rate limiting check - returns true if allowed, false if rate limited
static bool check_rate_limit(const char *ip_address) {
    time_t now = time(NULL);
    
    // Find or create entry for this IP
    int slot = -1;
    for (int i = 0; i < MAX_TRACKED_IPS; i++) {
        if (rate_limits[i].used && strcmp(rate_limits[i].ip_address, ip_address) == 0) {
            slot = i;
            break;
        }
        if (!rate_limits[i].used && slot == -1) {
            slot = i;
        }
    }
    
    if (slot == -1) {
        // No slots available - fail open (allow request)
        fprintf(stderr, "WARNING: Rate limit table full, allowing request from %s\n", ip_address);
        return true;
    }
    
    rate_limit_entry_t *entry = &rate_limits[slot];
    
    // Initialize new entry
    if (!entry->used) {
        strncpy(entry->ip_address, ip_address, 45);
        entry->ip_address[45] = '\0';
        entry->window_start = now;
        entry->request_count = 1;
        entry->used = 1;
        return true;
    }
    
    // Check if window expired - reset counter
    if (now - entry->window_start >= RATE_LIMIT_WINDOW) {
        entry->window_start = now;
        entry->request_count = 1;
        return true;
    }
    
    // Increment counter and check limit
    entry->request_count++;
    if (entry->request_count > MAX_REQUESTS_PER_WINDOW) {
        fprintf(stderr, "RATE LIMIT: Blocked %s (%d requests in %ld seconds)\n", 
                ip_address, entry->request_count, now - entry->window_start);
        return false;
    }
    
    return true;
}

// MIME type mapping
static const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    
    return "application/octet-stream";
}

// Base64 URL-safe encoding (for PKCE)
static char* base64url_encode(const unsigned char *input, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);
    
    // Save length before freeing BIO
    size_t result_length = buffer_ptr->length;
    char *result = malloc(result_length + 1);
    memcpy(result, buffer_ptr->data, result_length);
    result[result_length] = '\0';
    
    BIO_free_all(bio);
    
    // Convert to URL-safe (replace + with -, / with _, remove =)
    for (size_t i = 0; i < result_length; i++) {
        if (result[i] == '+') result[i] = '-';
        else if (result[i] == '/') result[i] = '_';
        else if (result[i] == '=') { result[i] = '\0'; break; }
    }
    
    return result;
}

// Generate PKCE code_verifier (128 random characters)
static void generate_code_verifier(char *verifier, size_t len) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    static unsigned int seed = 0;
    
    if (seed == 0) {
        seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
        srand(seed);
    }
    
    for (size_t i = 0; i < len - 1; i++) {
        verifier[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    verifier[len - 1] = '\0';
}

// Generate PKCE code_challenge from code_verifier (SHA256 hash, base64url encoded)
static char* generate_code_challenge(const char *verifier) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)verifier, strlen(verifier), hash);
    return base64url_encode(hash, SHA256_DIGEST_LENGTH);
}

// Add CORS headers
static void add_cors_headers(struct MHD_Response *response) {
    if (server_config.cors_enabled) {
        // Restrict CORS to our domain only - prevents cross-origin scraping
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "https://weather.limani-parou.com");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
        MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
    }
}

// Get session ID from cookie
static const char* get_session_cookie(struct MHD_Connection *connection) {
    const char *cookie_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Cookie");
    if (!cookie_header) return NULL;
    
    // Simple cookie parsing - look for session_id=value
    const char *session_start = strstr(cookie_header, "session_id=");
    if (!session_start) return NULL;
    
    session_start += strlen("session_id=");
    static char session_id[MAX_SESSION_ID_LENGTH];
    
    int i = 0;
    while (session_start[i] && session_start[i] != ';' && session_start[i] != ' ' && i < MAX_SESSION_ID_LENGTH - 1) {
        session_id[i] = session_start[i];
        i++;
    }
    session_id[i] = '\0';
    
    return session_id;
}

// Get current user profile from session or default
static user_profile_t* get_current_profile(struct MHD_Connection *connection) {
    const char *session_id = get_session_cookie(connection);
    if (session_id) {
        user_session_t *session = session_get(session_id);
        if (session && session->is_active) {
            // Check if token needs refresh (if we have tokens)
            if (session->token_expires_at > 0 && session->token_expires_at < time(NULL)) {
                printf("Access token expired for session %s, attempting refresh...\n", session_id);
                if (!session_refresh_tokens(session_id)) {
                    printf("Token refresh failed, session invalidated\n");
                    session->is_active = false;
                    return profile_get_default();
                }
            }
            
            return profile_get_for_user(session->user_id);
        }
    }
    return profile_get_default();
}

// Create JSON error response
static struct MHD_Response* create_error_response(int code, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(json, "error", error);
    
    char *json_string = cJSON_Print(json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    cJSON_Delete(json);
    return response;
}

// Handle profile GET request
static enum MHD_Result handle_profile_get(struct MHD_Connection *connection) {
    user_profile_t *profile = get_current_profile(connection);
    
    cJSON *json = cJSON_CreateObject();
    
    cJSON_AddStringToObject(json, "userId", profile->user_id);
    cJSON_AddStringToObject(json, "name", profile->name);
    cJSON_AddBoolToObject(json, "isAuthenticated", profile->is_authenticated);
    cJSON_AddStringToObject(json, "tempUnit", 
        profile->temp_unit == TEMP_CELSIUS ? "celsius" : "fahrenheit");
    cJSON_AddStringToObject(json, "windUnit",
        profile->wind_unit == WIND_KMH ? "kmh" : 
        profile->wind_unit == WIND_KNOTS ? "knots" : "ms");
    cJSON_AddStringToObject(json, "defaultLocation", profile->default_location);
    
    char *json_string = cJSON_Print(json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    cJSON_Delete(json);
    
    return ret;
}

// Handle profile PUT request
static enum MHD_Result handle_profile_put(struct MHD_Connection *connection,
                                         const char *upload_data, size_t *upload_data_size) {
    (void)upload_data_size; // Not used anymore, handled by main handler
    
    user_profile_t *current_profile = get_current_profile(connection);
    
    // Parse JSON request
    cJSON *json = cJSON_Parse(upload_data);
    if (!json) {
        struct MHD_Response *response = create_error_response(400, "Invalid JSON");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Update profile fields
    user_profile_t updated_profile = *current_profile;
    
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *temp_unit = cJSON_GetObjectItem(json, "tempUnit");
    cJSON *wind_unit = cJSON_GetObjectItem(json, "windUnit");
    cJSON *default_location = cJSON_GetObjectItem(json, "defaultLocation");
    
    if (name && cJSON_IsString(name)) {
        strncpy(updated_profile.name, name->valuestring, MAX_NAME_LENGTH - 1);
        updated_profile.name[MAX_NAME_LENGTH - 1] = '\0';
    }
    
    if (temp_unit && cJSON_IsString(temp_unit)) {
        if (strcmp(temp_unit->valuestring, "fahrenheit") == 0) {
            updated_profile.temp_unit = TEMP_FAHRENHEIT;
        } else {
            updated_profile.temp_unit = TEMP_CELSIUS;
        }
    }

    if (wind_unit && cJSON_IsString(wind_unit)) {
        if (strcmp(wind_unit->valuestring, "knots") == 0) {
            updated_profile.wind_unit = WIND_KNOTS;
        } else if (strcmp(wind_unit->valuestring, "ms") == 0) {
            updated_profile.wind_unit = WIND_MS;
        } else {
            updated_profile.wind_unit = WIND_KMH;
        }
    }
    
    if (default_location && cJSON_IsString(default_location)) {
        strncpy(updated_profile.default_location, default_location->valuestring, MAX_LOCATION_LENGTH - 1);
        updated_profile.default_location[MAX_LOCATION_LENGTH - 1] = '\0';
    }
    
    // Save profile
    profile_update_for_user(current_profile->user_id, &updated_profile);
    
    cJSON_Delete(json);
    
    // Return updated profile
    return handle_profile_get(connection);
}

// Generate random state for OIDC with PKCE verifier
static char* generate_state(char **code_verifier_out) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char *state = malloc(65);
    char *code_verifier = malloc(129);
    
    srand(time(NULL) ^ (unsigned int)getpid());
    for (int i = 0; i < 64; i++) {
        state[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    state[64] = '\0';
    
    // Generate PKCE code_verifier
    generate_code_verifier(code_verifier, 129);
    
    // Store state and code_verifier together
    for (int i = 0; i < MAX_PENDING_STATES; i++) {
        if (!pending_states[i].used || difftime(time(NULL), pending_states[i].created) > 600) {
            strncpy(pending_states[i].state, state, 64);
            strncpy(pending_states[i].code_verifier, code_verifier, 128);
            pending_states[i].created = time(NULL);
            pending_states[i].used = 1;
            break;
        }
    }
    
    *code_verifier_out = code_verifier;
    return state;
}

// Validate and consume state, return code_verifier
static char* validate_and_get_verifier(const char *state) {
    for (int i = 0; i < MAX_PENDING_STATES; i++) {
        if (pending_states[i].used && strcmp(pending_states[i].state, state) == 0) {
            char *verifier = strdup(pending_states[i].code_verifier);
            pending_states[i].used = 0;  // Consume state
            return verifier;
        }
    }
    return NULL;
}

// Handle /api/auth/login - Redirect to OIDC provider
static enum MHD_Result handle_oidc_login(struct MHD_Connection *connection) {
    // Check if OIDC is configured
    if (!oidc_is_configured()) {
        struct MHD_Response *response = create_error_response(501, "OIDC authentication not configured");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_IMPLEMENTED, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    char *code_verifier = NULL;
    char *state = generate_state(&code_verifier);
    char *code_challenge = generate_code_challenge(code_verifier);
    char *auth_url = oidc_get_authorization_url(state, code_challenge);
    
    free(code_challenge);  // No longer needed after URL generation
    free(code_verifier);   // Stored in pending_states array
    
    if (!auth_url) {
        free(state);
        struct MHD_Response *response = create_error_response(500, "Failed to generate authorization URL");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Return JSON with redirect URL
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "redirectUrl", auth_url);
    
    char *json_string = cJSON_Print(response_json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    
    MHD_destroy_response(response);
    cJSON_Delete(response_json);
    free(auth_url);
    free(state);
    
    return ret;
}

// Handle /api/auth/callback - Process OIDC callback
static enum MHD_Result handle_oidc_callback(struct MHD_Connection *connection) {
    // Check if OIDC is configured
    if (!oidc_is_configured()) {
        struct MHD_Response *response = create_error_response(501, "OIDC authentication not configured");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_IMPLEMENTED, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    const char *code = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code");
    const char *state = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "state");
    const char *error = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "error");
    
    // Handle error from provider
    if (error) {
        printf("OIDC error: %s\n", error);
        // Redirect to frontend with error
        const char *redirect = "/#/login?error=auth_failed";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            0, "", MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Location", redirect);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Validate parameters
    if (!code || !state) {
        struct MHD_Response *response = create_error_response(400, "Missing code or state");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Validate state and get code_verifier
    char *code_verifier = validate_and_get_verifier(state);
    if (!code_verifier) {
        printf("Invalid or expired state: %s\n", state);
        struct MHD_Response *response = create_error_response(400, "Invalid state");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Exchange code for tokens with PKCE verifier
    oidc_tokens_t *tokens = oidc_exchange_code(code, code_verifier);
    free(code_verifier);
    
    if (!tokens) {
        printf("Failed to exchange code for tokens\n");
        const char *redirect = "/#/login?error=token_exchange_failed";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            0, "", MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Location", redirect);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Get user info
    oidc_userinfo_t *userinfo = oidc_get_userinfo(tokens->access_token);
    if (!userinfo) {
        printf("Failed to get user info\n");
        oidc_free_tokens(tokens);
        const char *redirect = "/#/login?error=userinfo_failed";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            0, "", MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Location", redirect);
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    printf("User authenticated: %s (%s)\n", userinfo->name ? userinfo->name : "Unknown", userinfo->sub);
    
    // Create session with user ID from OIDC (sub claim)
    const char *session_id = session_create(userinfo->sub, userinfo->name ? userinfo->name : userinfo->preferred_username);
    
    // Store OIDC tokens in session
    session_store_tokens(session_id, 
                        tokens->access_token, 
                        tokens->refresh_token, 
                        tokens->id_token, 
                        tokens->expires_in);
    
    oidc_free_userinfo(userinfo);
    oidc_free_tokens(tokens);
    
    // Redirect to frontend with session cookie (Secure flag for HTTPS)
    const char *redirect = "/";
    struct MHD_Response *response = MHD_create_response_from_buffer(
        0, "", MHD_RESPMEM_PERSISTENT);
    
    // Set session cookie with Secure flag
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "session_id=%s; Path=/; HttpOnly; Secure; SameSite=Lax", session_id);
    MHD_add_response_header(response, "Set-Cookie", cookie);
    MHD_add_response_header(response, "Location", redirect);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle login POST request (DEPRECATED - use /api/auth/login instead)
static enum MHD_Result handle_login(struct MHD_Connection *connection,
                                    const char *upload_data, size_t *upload_data_size) {
    (void)upload_data_size; // Not used anymore, handled by main handler
    
    // Parse JSON request
    cJSON *json = cJSON_Parse(upload_data);
    if (!json) {
        struct MHD_Response *response = create_error_response(400, "Invalid JSON");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    cJSON *user_id = cJSON_GetObjectItem(json, "userId");
    cJSON *name = cJSON_GetObjectItem(json, "name");
    
    if (!user_id || !cJSON_IsString(user_id) || !name || !cJSON_IsString(name)) {
        cJSON_Delete(json);
        struct MHD_Response *response = create_error_response(400, "Missing userId or name");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Create session
    const char *session_id = session_create(user_id->valuestring, name->valuestring);
    
    // Create response with session cookie
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(response_json, "success", true);
    cJSON_AddStringToObject(response_json, "userId", user_id->valuestring);
    cJSON_AddStringToObject(response_json, "name", name->valuestring);
    cJSON_AddStringToObject(response_json, "sessionId", session_id);
    
    char *json_string = cJSON_Print(response_json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    
    // Set session cookie
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "session_id=%s; Path=/; HttpOnly; SameSite=Lax", session_id);
    MHD_add_response_header(response, "Set-Cookie", cookie);
    
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    cJSON_Delete(json);
    cJSON_Delete(response_json);
    
    return ret;
}

// Handle logout POST request
static enum MHD_Result handle_logout(struct MHD_Connection *connection) {
    const char *session_id = get_session_cookie(connection);
    
    if (session_id) {
        session_destroy(session_id);
    }
    
    // Create response
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(response_json, "success", true);
    
    char *json_string = cJSON_Print(response_json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_string), json_string, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    
    // Clear session cookie
    MHD_add_response_header(response, "Set-Cookie", "session_id=; Path=/; HttpOnly; Max-Age=0");
    
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    cJSON_Delete(response_json);
    
    return ret;
}

// Handle weather current request
static enum MHD_Result handle_weather_current(struct MHD_Connection *connection) {
    // Security: Rate limiting by IP address (applied to all users including guests)
    const char *client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Forwarded-For");
    if (!client_ip) {
        client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Real-IP");
    }
    if (!client_ip) {
        client_ip = "unknown";
    }
    
    if (!check_rate_limit(client_ip)) {
        struct MHD_Response *response = create_error_response(429, "Too many requests. Please try again later.");
        enum MHD_Result ret = MHD_queue_response(connection, 429, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
    const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
    
    if (!location) {
        struct MHD_Response *response = create_error_response(400, "Missing 'location' parameter");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    bool include_aqi = aqi_str && strcmp(aqi_str, "true") == 0;
    
    char *weather_response;
    int result = weather_client_get_current(location, include_aqi, &weather_response);
    
    if (result != 0) {
        struct MHD_Response *response = create_error_response(500, "Failed to fetch weather data");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(weather_response), weather_response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle weather forecast request
static enum MHD_Result handle_weather_forecast(struct MHD_Connection *connection) {
    // Security: Rate limiting by IP address (applied to all users including guests)
    const char *client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Forwarded-For");
    if (!client_ip) {
        client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Real-IP");
    }
    if (!client_ip) {
        client_ip = "unknown";
    }
    
    if (!check_rate_limit(client_ip)) {
        struct MHD_Response *response = create_error_response(429, "Too many requests. Please try again later.");
        enum MHD_Result ret = MHD_queue_response(connection, 429, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    const char *location = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "location");
    const char *days_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "days");
    const char *aqi_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_aqi");
    const char *alerts_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_alerts");
    const char *hourly_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "include_hourly");
    
    if (!location || !days_str) {
        struct MHD_Response *response = create_error_response(400, "Missing 'location' or 'days' parameter");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    weather_request_t request;
    strncpy(request.location, location, MAX_LOCATION_LENGTH - 1);
    request.location[MAX_LOCATION_LENGTH - 1] = '\0';
    request.days = atoi(days_str);
    request.include_aqi = aqi_str && strcmp(aqi_str, "true") == 0;
    request.include_alerts = alerts_str && strcmp(alerts_str, "true") == 0;
    request.include_hourly = hourly_str && strcmp(hourly_str, "true") == 0;
    
    if (request.days < 1 || request.days > 14) {
        struct MHD_Response *response = create_error_response(400, "Invalid days parameter (must be 1-14)");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    char *weather_response;
    int result = weather_client_get_forecast(&request, &weather_response);
    
    if (result != 0) {
        struct MHD_Response *response = create_error_response(500, "Failed to fetch forecast data");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(weather_response), weather_response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Serve static files
static enum MHD_Result serve_static_file(struct MHD_Connection *connection, const char *url) {
    char filepath[512];
    
    // Handle root path
    if (strcmp(url, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", server_config.static_path);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", server_config.static_path, url);
    }
    
    // Security check - prevent directory traversal
    if (strstr(filepath, "..") != NULL) {
        struct MHD_Response *response = create_error_response(403, "Access denied");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FORBIDDEN, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Open file
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        struct MHD_Response *response = create_error_response(404, "File not found");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Get file size
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
        struct MHD_Response *response = create_error_response(500, "File stat error");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_fd(file_stat.st_size, fd);
    if (!response) {
        close(fd);
        struct MHD_Response *error_response = create_error_response(500, "Failed to create response");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, error_response);
        MHD_destroy_response(error_response);
        return ret;
    }
    
    // Set content type
    const char *mime_type = get_mime_type(filepath);
    MHD_add_response_header(response, "Content-Type", mime_type);
    add_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Main request handler
static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                          const char *url, const char *method,
                                          const char *version, const char *upload_data,
                                          size_t *upload_data_size, void **con_cls) {
    (void)cls; (void)version;
    
    // First call - initialize context
    if (*con_cls == NULL) {
        struct request_context *ctx = malloc(sizeof(struct request_context));
        if (!ctx) return MHD_NO;
        ctx->data = NULL;
        ctx->size = 0;
        *con_cls = ctx;
        return MHD_YES;
    }
    
    struct request_context *ctx = *con_cls;
    
    // Accumulate POST data
    if (*upload_data_size > 0) {
        char *new_data = realloc(ctx->data, ctx->size + *upload_data_size + 1);
        if (!new_data) {
            free(ctx->data);
            free(ctx);
            return MHD_NO;
        }
        ctx->data = new_data;
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        ctx->data[ctx->size] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    if (server_config.verbose) {
        printf("Request: %s %s\n", method, url);
    }
    
    enum MHD_Result result;
    
    // Handle OPTIONS for CORS
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        add_cors_headers(response);
        result = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        free(ctx->data);
        free(ctx);
        return result;
    }
    
    // API routes
    if (strncmp(url, "/api/", 5) == 0) {
        if (strcmp(url, "/api/profile") == 0) {
            if (strcmp(method, "GET") == 0) {
                result = handle_profile_get(connection);
            } else if (strcmp(method, "PUT") == 0 || strcmp(method, "POST") == 0) {
                size_t data_size = ctx->size;
                result = handle_profile_put(connection, ctx->data, &data_size);
            } else {
                struct MHD_Response *response = create_error_response(405, "Method not allowed");
                result = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
                MHD_destroy_response(response);
            }
        } else if (strcmp(url, "/api/auth/login") == 0 && strcmp(method, "GET") == 0) {
            result = handle_oidc_login(connection);
        } else if (strcmp(url, "/api/auth/callback") == 0 && strcmp(method, "GET") == 0) {
            result = handle_oidc_callback(connection);
        } else if (strcmp(url, "/api/login") == 0 && strcmp(method, "POST") == 0) {
            // Deprecated: kept for backwards compatibility during transition
            size_t data_size = ctx->size;
            result = handle_login(connection, ctx->data, &data_size);
        } else if (strcmp(url, "/api/logout") == 0 && strcmp(method, "POST") == 0) {
            result = handle_logout(connection);
        } else if (strcmp(url, "/api/weather/current") == 0 && strcmp(method, "GET") == 0) {
            result = handle_weather_current(connection);
        } else if (strcmp(url, "/api/weather/forecast") == 0 && strcmp(method, "GET") == 0) {
            result = handle_weather_forecast(connection);
        } else {
            // API endpoint not found
            struct MHD_Response *response = create_error_response(404, "API endpoint not found");
            result = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);
        }
        free(ctx->data);
        free(ctx);
        return result;
    }
    
    // Serve static files
    if (strcmp(method, "GET") == 0) {
        result = serve_static_file(connection, url);
        free(ctx->data);
        free(ctx);
        return result;
    }
    
    // Method not allowed
    struct MHD_Response *response = create_error_response(405, "Method not allowed");
    result = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
    MHD_destroy_response(response);
    free(ctx->data);
    free(ctx);
    return result;
}

// Profile management functions
int profile_init(void) {
    // Initialization now handled by session_manager
    return 0;
}

user_profile_t* profile_get_current(void) {
    // Deprecated - use get_current_profile with connection
    return profile_get_default();
}

int profile_update(const user_profile_t *profile) {
    // Deprecated - use profile_update_for_user
    return profile_update_for_user("", profile);
}

void profile_cleanup(void) {
    // Nothing to cleanup for in-memory storage
}

// Server functions
int http_server_start(const server_config_t *config) {
    if (!config) {
        fprintf(stderr, "Invalid server configuration\n");
        return -1;
    }
    
    memcpy(&server_config, config, sizeof(server_config_t));
    
    // Initialize database connection
    if (db_manager_init(config->database_url) != 0) {
        fprintf(stderr, "Failed to initialize database connection\n");
        return -1;
    }
    
    // Initialize session manager (includes default profile)
    if (session_manager_init() != 0) {
        fprintf(stderr, "Failed to initialize session manager\n");
        db_manager_cleanup();
        return -1;
    }
    
    // Initialize profile (legacy support)
    if (profile_init() != 0) {
        fprintf(stderr, "Failed to initialize user profile\n");
        db_manager_cleanup();
        return -1;
    }
    
    // Initialize weather client
    if (weather_client_init(config->weather_service_url) != 0) {
        fprintf(stderr, "Failed to initialize weather client\n");
        db_manager_cleanup();
        return -1;
    }
    
    // Start HTTP daemon
    http_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
                             config->port,
                             NULL, NULL,
                             &answer_to_connection, NULL,
                             MHD_OPTION_END);
    
    if (!http_daemon) {
        fprintf(stderr, "Failed to start HTTP server on port %d\n", config->port);
        weather_client_cleanup();
        db_manager_cleanup();
        return -1;
    }
    
    printf("Weather Dashboard Server started on %s:%d\n", config->bind_address, config->port);
    printf("Static files served from: %s\n", config->static_path);
    printf("Weather service URL: %s\n", config->weather_service_url);
    printf("Database: %s\n", db_is_connected() ? "Connected" : "Not connected");
    printf("CORS enabled: %s\n", config->cors_enabled ? "Yes" : "No");
    printf("Verbose logging: %s\n", config->verbose ? "Yes" : "No");
    
    return 0;
}

void http_server_stop(void) {
    if (http_daemon) {
        printf("Stopping server...\n");
        MHD_stop_daemon(http_daemon);
        http_daemon = NULL;
    }
    
    weather_client_cleanup();
    profile_cleanup();
    session_manager_cleanup();
    db_manager_cleanup();
    printf("Server stopped.\n");
}