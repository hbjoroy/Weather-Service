# Security Improvements for Weather Dashboard

## Current Security Status

### What's Working
✅ Session-based authentication with OIDC
✅ User isolation (users can only access their own profiles)
✅ HttpOnly cookies (XSS protection)
✅ SameSite=Lax (CSRF protection)
✅ OIDC state validation (OAuth CSRF protection)

### Security Gaps
❌ Access tokens discarded after authentication
❌ No session expiry
❌ No token refresh mechanism
❌ No Secure flag on cookies (HTTPS-only)
❌ Sessions persist even if Authentik revokes access

## Recommended Improvements

### Priority 1: Critical Security Fixes

#### 1. Add Secure Flag to Cookies
**Why:** Prevent session cookies from being transmitted over HTTP

```c
// In handle_oidc_callback and handle_login:
snprintf(cookie, sizeof(cookie), 
    "session_id=%s; Path=/; HttpOnly; Secure; SameSite=Lax", 
    session_id);
```

#### 2. Implement Session Expiry
**Why:** Limit the damage from stolen session cookies

```c
// Add to session structure:
typedef struct {
    // ... existing fields ...
    time_t expires_at;  // When session expires
} user_session_t;

// In session_create():
session->expires_at = time(NULL) + 3600; // 1 hour

// In session_get():
if (session->is_active && session->expires_at < time(NULL)) {
    session->is_active = false;
    return NULL;
}
```

#### 3. Store Access Token in Session
**Why:** Validate tokens are still valid with Authentik

```c
typedef struct {
    // ... existing fields ...
    char access_token[512];
    char refresh_token[512];
    time_t token_expires_at;
} user_session_t;

// In handle_oidc_callback():
// DON'T discard tokens - store them in session
session_store_tokens(session_id, tokens);
```

#### 4. Validate Token on Each Request
**Why:** Ensure user is still authorized by Authentik

```c
// In get_current_profile():
user_session_t *session = session_get(session_id);
if (session && session->is_active) {
    // Check if token is expired
    if (session->token_expires_at < time(NULL)) {
        // Try to refresh token
        if (!refresh_access_token(session)) {
            session->is_active = false;
            return profile_get_default();
        }
    }
    return profile_get_for_user(session->user_id);
}
```

### Priority 2: Enhanced Security

#### 5. Token Refresh Flow
**Why:** Keep sessions alive without re-authentication

```c
bool refresh_access_token(user_session_t *session) {
    oidc_tokens_t *new_tokens = oidc_refresh_token(session->refresh_token);
    if (!new_tokens) {
        return false;
    }
    
    // Update session with new tokens
    strncpy(session->access_token, new_tokens->access_token, 511);
    strncpy(session->refresh_token, new_tokens->refresh_token, 511);
    session->token_expires_at = time(NULL) + new_tokens->expires_in;
    
    oidc_free_tokens(new_tokens);
    return true;
}
```

#### 6. Implement Proper Logout
**Why:** Revoke tokens with Authentik, not just clear local session

```c
// In handle_logout():
user_session_t *session = session_get(session_id);
if (session && session->is_active) {
    // Revoke token with Authentik
    oidc_revoke_token(session->access_token);
    
    // Clear local session
    session_destroy(session_id);
}
```

#### 7. Add PKCE (Proof Key for Code Exchange)
**Why:** Additional protection against authorization code interception

```c
// Generate code_verifier and code_challenge before redirect
char *code_verifier = generate_pkce_verifier();
char *code_challenge = generate_pkce_challenge(code_verifier);

// Store code_verifier with state
// Send code_challenge in authorization URL
// Send code_verifier when exchanging code for tokens
```

### Priority 3: Optional Enhancements

#### 8. Rate Limiting
**Why:** Prevent brute force attacks on sessions

```c
// Track failed attempts per IP
// Block after N failed authentications in X minutes
```

#### 9. Session Fingerprinting
**Why:** Detect session hijacking

```c
typedef struct {
    // ... existing fields ...
    char user_agent_hash[64];
    char ip_address[46];  // IPv6 max length
} user_session_t;

// Validate fingerprint matches on each request
```

#### 10. Audit Logging
**Why:** Track authentication events for security monitoring

```c
// Log all login/logout events with:
// - Timestamp
// - User ID
// - IP address
// - Success/failure
// - Reason for failure
```

## Implementation Timeline

### Phase 1 (Immediate - Next Release)
1. Add Secure flag to cookies
2. Implement session expiry (1 hour)
3. Add session renewal endpoint

### Phase 2 (Next Sprint)
4. Store access/refresh tokens in sessions
5. Implement token refresh flow
6. Add token validation middleware

### Phase 3 (Future Enhancement)
7. Implement proper logout with token revocation
8. Add PKCE to OIDC flow
9. Add rate limiting
10. Implement audit logging

## Testing Checklist

After implementing improvements:

- [ ] Users can only access their own profiles
- [ ] Sessions expire after 1 hour of inactivity
- [ ] Tokens are refreshed automatically before expiry
- [ ] Logout revokes tokens with Authentik
- [ ] Session cookies only sent over HTTPS
- [ ] Stolen session cookies have limited lifetime
- [ ] Revoking access in Authentik invalidates app sessions
- [ ] Multiple sessions per user work correctly
- [ ] Session renewal works without re-login

## Security Best Practices

1. **Never trust client input** - Always validate session server-side
2. **Defense in depth** - Multiple layers of security (cookies + tokens + expiry)
3. **Least privilege** - Users can only access their own data
4. **Fail securely** - If validation fails, deny access (don't default to allowing)
5. **Monitor and log** - Track authentication events for anomaly detection
