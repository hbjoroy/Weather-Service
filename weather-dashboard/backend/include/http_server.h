#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "dashboard_types.h"

// Server lifecycle functions
int http_server_start(const server_config_t *config);
void http_server_stop(void);

// Profile management
int profile_init(void);
user_profile_t* profile_get_current(void);
int profile_update(const user_profile_t *profile);
void profile_cleanup(void);

#endif // HTTP_SERVER_H