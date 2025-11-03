#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "session_manager.h"
#include "db_manager.h"
#include "oidc_client.h"

#define MAX_SESSIONS 100
#define SESSION_DURATION 3600  // 1 hour in seconds

// Storage
static user_session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static user_profile_t default_profile;

// Generate a simple session ID
static void generate_session_id(char *session_id, size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static unsigned int seed = 0;
    
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
        srand(seed);
    }
    
    for (size_t i = 0; i < len - 1; i++) {
        session_id[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    session_id[len - 1] = '\0';
}

// Initialize session manager with default profile
int session_manager_init(void) {
    // Initialize default profile (anonymous/not logged in)
    memset(&default_profile, 0, sizeof(user_profile_t));
    strcpy(default_profile.user_id, "");  // Empty = default
    strcpy(default_profile.name, "Guest");
    default_profile.temp_unit = TEMP_CELSIUS;
    default_profile.wind_unit = WIND_MS;
    strcpy(default_profile.default_location, "Paros");
    default_profile.is_authenticated = false;
    
    session_count = 0;
    
    printf("Session manager initialized with default profile\n");
    return 0;
}

void session_manager_cleanup(void) {
    session_count = 0;
}

// Create a new session
const char* session_create(const char *user_id, const char *user_name) {
    if (session_count >= MAX_SESSIONS) {
        // Simple cleanup: remove oldest session
        session_count = 0;
    }
    
    user_session_t *session = &sessions[session_count];
    session_count++;
    
    generate_session_id(session->session_id, MAX_SESSION_ID_LENGTH);
    strncpy(session->user_id, user_id, MAX_USER_ID_LENGTH - 1);
    session->user_id[MAX_USER_ID_LENGTH - 1] = '\0';
    session->created_at = time(NULL);
    session->last_accessed = session->created_at;
    session->expires_at = session->created_at + SESSION_DURATION;  // 1 hour from now
    session->is_active = true;
    
    // Initialize token fields as empty
    session->access_token[0] = '\0';
    session->refresh_token[0] = '\0';
    session->id_token[0] = '\0';
    session->token_expires_at = 0;
    
    // Try to load profile from database
    user_profile_t profile;
    int result = db_load_profile(user_id, &profile);
    
    if (result == 1) {
        // User not found in database - create new profile
        memset(&profile, 0, sizeof(user_profile_t));
        strncpy(profile.user_id, user_id, MAX_USER_ID_LENGTH - 1);
        profile.user_id[MAX_USER_ID_LENGTH - 1] = '\0';
        strncpy(profile.name, user_name, MAX_NAME_LENGTH - 1);
        profile.name[MAX_NAME_LENGTH - 1] = '\0';
        profile.is_authenticated = true;
        
        // Copy default settings
        profile.temp_unit = default_profile.temp_unit;
        profile.wind_unit = default_profile.wind_unit;
        strcpy(profile.default_location, default_profile.default_location);
        
        // Save to database
        db_save_profile(&profile);
    } else if (result == 0) {
        // Profile loaded successfully - update name if changed
        if (strcmp(profile.name, user_name) != 0) {
            strncpy(profile.name, user_name, MAX_NAME_LENGTH - 1);
            profile.name[MAX_NAME_LENGTH - 1] = '\0';
            profile.is_authenticated = true;
            db_save_profile(&profile);
        }
    }
    
    printf("Created session %s for user %s (%s)\n", session->session_id, user_id, user_name);
    return session->session_id;
}

// Get session by ID
user_session_t* session_get(const char *session_id) {
    if (!session_id) return NULL;
    
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].is_active && strcmp(sessions[i].session_id, session_id) == 0) {
            // Check if session has expired
            if (sessions[i].expires_at < time(NULL)) {
                sessions[i].is_active = false;
                printf("Session %s expired\n", session_id);
                return NULL;
            }
            
            sessions[i].last_accessed = time(NULL);
            // Extend session expiry on access
            sessions[i].expires_at = time(NULL) + SESSION_DURATION;
            return &sessions[i];
        }
    }
    return NULL;
}

// Store OIDC tokens in session
void session_store_tokens(const char *session_id, const char *access_token, 
                          const char *refresh_token, const char *id_token, int expires_in) {
    if (!session_id) return;
    
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].is_active && strcmp(sessions[i].session_id, session_id) == 0) {
            if (access_token) {
                strncpy(sessions[i].access_token, access_token, MAX_TOKEN_LENGTH - 1);
                sessions[i].access_token[MAX_TOKEN_LENGTH - 1] = '\0';
            }
            if (refresh_token) {
                strncpy(sessions[i].refresh_token, refresh_token, MAX_TOKEN_LENGTH - 1);
                sessions[i].refresh_token[MAX_TOKEN_LENGTH - 1] = '\0';
            }
            if (id_token) {
                strncpy(sessions[i].id_token, id_token, MAX_TOKEN_LENGTH - 1);
                sessions[i].id_token[MAX_TOKEN_LENGTH - 1] = '\0';
            }
            
            // Set token expiry time (slightly earlier to be safe)
            sessions[i].token_expires_at = time(NULL) + expires_in - 60;
            
            printf("Stored tokens for session %s (expires in %d seconds)\n", session_id, expires_in);
            return;
        }
    }
}

// Refresh session tokens using refresh token
bool session_refresh_tokens(const char *session_id) {
    if (!session_id) return false;
    
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].is_active && strcmp(sessions[i].session_id, session_id) == 0) {
            // Check if we have a refresh token
            if (sessions[i].refresh_token[0] == '\0') {
                return false;  // No refresh token available
            }
            
            // Try to refresh
            oidc_tokens_t *new_tokens = oidc_refresh_token(sessions[i].refresh_token);
            if (!new_tokens) {
                printf("Failed to refresh tokens for session %s\n", session_id);
                return false;
            }
            
            // Update tokens
            session_store_tokens(session_id, 
                                new_tokens->access_token,
                                new_tokens->refresh_token ? new_tokens->refresh_token : sessions[i].refresh_token,
                                new_tokens->id_token,
                                new_tokens->expires_in);
            
            oidc_free_tokens(new_tokens);
            printf("Refreshed tokens for session %s\n", session_id);
            return true;
        }
    }
    return false;
}

// Destroy session
void session_destroy(const char *session_id) {
    if (!session_id) return;
    
    for (int i = 0; i < session_count; i++) {
        if (strcmp(sessions[i].session_id, session_id) == 0) {
            sessions[i].is_active = false;
            printf("Destroyed session %s\n", session_id);
            return;
        }
    }
}

// Get default profile
user_profile_t* profile_get_default(void) {
    return &default_profile;
}

// Get profile for user
user_profile_t* profile_get_for_user(const char *user_id) {
    if (!user_id || strlen(user_id) == 0) {
        return &default_profile;
    }
    
    // Try to load from database
    static user_profile_t loaded_profile;
    int result = db_load_profile(user_id, &loaded_profile);
    
    if (result == 0) {
        // Successfully loaded from database
        return &loaded_profile;
    }
    
    // Not found or error - return default
    return &default_profile;
}

// Update profile for user
int profile_update_for_user(const char *user_id, const user_profile_t *profile) {
    if (!profile) return -1;
    
    if (!user_id || strlen(user_id) == 0) {
        // Can't update default profile
        return -1;
    }
    
    // Save to database
    return db_save_profile(profile);
}
