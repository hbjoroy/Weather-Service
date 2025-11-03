#ifndef OIDC_CLIENT_H
#define OIDC_CLIENT_H

#include <stdbool.h>

// OIDC configuration
typedef struct {
    char *issuer;              // e.g., "https://identity.limani-parou.com/application/o/limani-weather/"
    char *client_id;
    char *client_secret;
    char *redirect_uri;        // e.g., "https://weather.limani-parou.com/api/auth/callback"
    char *authorization_endpoint;
    char *token_endpoint;
    char *userinfo_endpoint;
    char *end_session_endpoint;
} oidc_config_t;

// OIDC token response
typedef struct {
    char *access_token;
    char *id_token;
    char *refresh_token;
    int expires_in;
    char *token_type;
} oidc_tokens_t;

// User info from OIDC provider
typedef struct {
    char *sub;           // Subject (unique user ID)
    char *name;
    char *email;
    char *preferred_username;
} oidc_userinfo_t;

// Initialize OIDC client
bool oidc_init(const char *issuer, const char *client_id, const char *client_secret, const char *redirect_uri);

// Check if OIDC is initialized and configured
bool oidc_is_configured(void);

// Get authorization URL for user login (with PKCE)
char* oidc_get_authorization_url(const char *state, const char *code_challenge);

// Exchange authorization code for tokens (with PKCE)
oidc_tokens_t* oidc_exchange_code(const char *code, const char *code_verifier);

// Refresh access token using refresh token
oidc_tokens_t* oidc_refresh_token(const char *refresh_token);

// Get user info using access token
oidc_userinfo_t* oidc_get_userinfo(const char *access_token);

// Get logout URL
char* oidc_get_logout_url(const char *id_token_hint, const char *post_logout_redirect_uri);

// Free tokens structure
void oidc_free_tokens(oidc_tokens_t *tokens);

// Free userinfo structure
void oidc_free_userinfo(oidc_userinfo_t *userinfo);

// Cleanup OIDC client
void oidc_cleanup(void);

#endif // OIDC_CLIENT_H
