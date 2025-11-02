#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "dashboard_types.h"

// Initialize database connection
// connection_string format: "host=localhost port=5432 dbname=bjosoft-weather user=weather password=weather123"
int db_manager_init(const char *connection_string);

// Cleanup database connection
void db_manager_cleanup(void);

// Load user profile from database
// Returns 0 on success, -1 on error, 1 if user not found
int db_load_profile(const char *user_id, user_profile_t *profile);

// Save user profile to database (insert or update)
// Returns 0 on success, -1 on error
int db_save_profile(const user_profile_t *profile);

// Delete user profile from database
// Returns 0 on success, -1 on error
int db_delete_profile(const char *user_id);

// Check if database is connected
bool db_is_connected(void);

#endif // DB_MANAGER_H
