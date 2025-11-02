#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include "dashboard_types.h"

// Initialize session manager
int session_manager_init(void);

// Cleanup session manager
void session_manager_cleanup(void);

// Create a new session for a user
const char* session_create(const char *user_id, const char *user_name);

// Get session by session_id (from cookie)
user_session_t* session_get(const char *session_id);

// Destroy a session (logout)
void session_destroy(const char *session_id);

// Get or create default profile
user_profile_t* profile_get_default(void);

// Get profile for user (returns default if not found or not authenticated)
user_profile_t* profile_get_for_user(const char *user_id);

// Update profile for user
int profile_update_for_user(const char *user_id, const user_profile_t *profile);

#endif // SESSION_MANAGER_H
