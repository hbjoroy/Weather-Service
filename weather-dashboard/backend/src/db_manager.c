#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <cjson/cJSON.h>
#include "db_manager.h"

static PGconn *db_conn = NULL;

// Initialize database connection
int db_manager_init(const char *connection_string) {
    if (!connection_string) {
        fprintf(stderr, "Database connection string is NULL\n");
        return -1;
    }
    
    db_conn = PQconnectdb(connection_string);
    
    if (PQstatus(db_conn) != CONNECTION_OK) {
        fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(db_conn));
        PQfinish(db_conn);
        db_conn = NULL;
        return -1;
    }
    
    printf("Database connected successfully\n");
    printf("PostgreSQL server version: %d\n", PQserverVersion(db_conn));
    
    return 0;
}

// Cleanup database connection
void db_manager_cleanup(void) {
    if (db_conn) {
        PQfinish(db_conn);
        db_conn = NULL;
        printf("Database connection closed\n");
    }
}

// Check if database is connected
bool db_is_connected(void) {
    return db_conn != NULL && PQstatus(db_conn) == CONNECTION_OK;
}

// Parse JSON profile data into user_profile_t
static int parse_profile_json(const char *json_str, user_profile_t *profile) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        fprintf(stderr, "Failed to parse profile JSON: %s\n", json_str);
        return -1;
    }
    
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *temp_unit = cJSON_GetObjectItem(json, "tempUnit");
    cJSON *wind_unit = cJSON_GetObjectItem(json, "windUnit");
    cJSON *default_location = cJSON_GetObjectItem(json, "defaultLocation");
    cJSON *is_authenticated = cJSON_GetObjectItem(json, "isAuthenticated");
    
    if (name && cJSON_IsString(name)) {
        strncpy(profile->name, name->valuestring, MAX_NAME_LENGTH - 1);
        profile->name[MAX_NAME_LENGTH - 1] = '\0';
    }
    
    if (temp_unit && cJSON_IsString(temp_unit)) {
        profile->temp_unit = (strcmp(temp_unit->valuestring, "fahrenheit") == 0) 
            ? TEMP_FAHRENHEIT : TEMP_CELSIUS;
    }
    
    if (wind_unit && cJSON_IsString(wind_unit)) {
        if (strcmp(wind_unit->valuestring, "knots") == 0) {
            profile->wind_unit = WIND_KNOTS;
        } else if (strcmp(wind_unit->valuestring, "ms") == 0) {
            profile->wind_unit = WIND_MS;
        } else {
            profile->wind_unit = WIND_KMH;
        }
    }
    
    if (default_location && cJSON_IsString(default_location)) {
        strncpy(profile->default_location, default_location->valuestring, MAX_LOCATION_LENGTH - 1);
        profile->default_location[MAX_LOCATION_LENGTH - 1] = '\0';
    }
    
    if (is_authenticated && cJSON_IsBool(is_authenticated)) {
        profile->is_authenticated = cJSON_IsTrue(is_authenticated);
    }
    
    cJSON_Delete(json);
    return 0;
}

// Create JSON string from user_profile_t
static char* create_profile_json(const user_profile_t *profile) {
    cJSON *json = cJSON_CreateObject();
    
    cJSON_AddStringToObject(json, "name", profile->name);
    cJSON_AddStringToObject(json, "tempUnit", 
        profile->temp_unit == TEMP_CELSIUS ? "celsius" : "fahrenheit");
    cJSON_AddStringToObject(json, "windUnit",
        profile->wind_unit == WIND_KMH ? "kmh" : 
        profile->wind_unit == WIND_KNOTS ? "knots" : "ms");
    cJSON_AddStringToObject(json, "defaultLocation", profile->default_location);
    cJSON_AddBoolToObject(json, "isAuthenticated", profile->is_authenticated);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return json_str;
}

// Load user profile from database
int db_load_profile(const char *user_id, user_profile_t *profile) {
    if (!db_is_connected()) {
        fprintf(stderr, "Database not connected\n");
        return -1;
    }
    
    if (!user_id || !profile) {
        fprintf(stderr, "Invalid arguments to db_load_profile\n");
        return -1;
    }
    
    const char *query = "SELECT profile_data FROM user_profiles WHERE user_id = $1";
    const char *param_values[1] = { user_id };
    
    PGresult *res = PQexecParams(db_conn, query, 1, NULL, param_values, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Failed to load profile for user '%s': %s\n", user_id, PQerrorMessage(db_conn));
        PQclear(res);
        return -1;
    }
    
    if (PQntuples(res) == 0) {
        // User not found
        PQclear(res);
        return 1;
    }
    
    const char *json_str = PQgetvalue(res, 0, 0);
    
    // Set user_id
    strncpy(profile->user_id, user_id, MAX_USER_ID_LENGTH - 1);
    profile->user_id[MAX_USER_ID_LENGTH - 1] = '\0';
    
    // Parse JSON data
    int result = parse_profile_json(json_str, profile);
    
    PQclear(res);
    
    if (result == 0) {
        printf("Loaded profile for user '%s' from database\n", user_id);
    }
    
    return result;
}

// Save user profile to database
int db_save_profile(const user_profile_t *profile) {
    if (!db_is_connected()) {
        fprintf(stderr, "Database not connected\n");
        return -1;
    }
    
    if (!profile) {
        fprintf(stderr, "Invalid profile argument\n");
        return -1;
    }
    
    char *json_str = create_profile_json(profile);
    if (!json_str) {
        fprintf(stderr, "Failed to create JSON for profile\n");
        return -1;
    }
    
    const char *query = 
        "INSERT INTO user_profiles (user_id, profile_data) "
        "VALUES ($1, $2::jsonb) "
        "ON CONFLICT (user_id) DO UPDATE SET profile_data = $2::jsonb";
    
    const char *param_values[2] = { profile->user_id, json_str };
    
    PGresult *res = PQexecParams(db_conn, query, 2, NULL, param_values, NULL, NULL, 0);
    
    int result = 0;
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to save profile for user '%s': %s\n", profile->user_id, PQerrorMessage(db_conn));
        result = -1;
    } else {
        printf("Saved profile for user '%s' to database\n", profile->user_id);
    }
    
    PQclear(res);
    free(json_str);
    
    return result;
}

// Delete user profile from database
int db_delete_profile(const char *user_id) {
    if (!db_is_connected()) {
        fprintf(stderr, "Database not connected\n");
        return -1;
    }
    
    if (!user_id) {
        fprintf(stderr, "Invalid user_id argument\n");
        return -1;
    }
    
    const char *query = "DELETE FROM user_profiles WHERE user_id = $1";
    const char *param_values[1] = { user_id };
    
    PGresult *res = PQexecParams(db_conn, query, 1, NULL, param_values, NULL, NULL, 0);
    
    int result = 0;
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Failed to delete profile for user '%s': %s\n", user_id, PQerrorMessage(db_conn));
        result = -1;
    } else {
        printf("Deleted profile for user '%s' from database\n", user_id);
    }
    
    PQclear(res);
    
    return result;
}
