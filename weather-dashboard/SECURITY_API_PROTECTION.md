# API Protection Strategies for Weather Dashboard

## Current Vulnerabilities

### 1. Open CORS Policy
- **Current:** `Access-Control-Allow-Origin: *`
- **Risk:** Any website can make requests to your API from browsers
- **Impact:** Screen scraping, abuse of WeatherAPI.com quota

### 2. Unauthenticated Weather Endpoints
- **Current:** `/api/weather/current` and `/api/weather/forecast` don't require authentication
- **Risk:** Direct API access without login
- **Impact:** Unlimited weather data access

### 3. No Rate Limiting
- **Current:** No request limits
- **Risk:** Denial of service, quota exhaustion
- **Impact:** High costs, service disruption

## Protection Strategies

### Priority 1: Critical (Implement Immediately)

#### 1.1 Require Authentication for Weather Endpoints
Force users to be logged in before accessing weather data.

**Implementation:**
```c
static enum MHD_Result handle_weather_current(struct MHD_Connection *connection) {
    // CHECK SESSION FIRST
    const char *session_id = get_session_cookie(connection);
    user_session_t *session = session_get(session_id);
    
    if (!session || !session->is_active) {
        struct MHD_Response *response = create_error_response(401, "Authentication required");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Continue with normal logic...
}
```

**Benefits:**
- Prevents anonymous API access
- Ties usage to authenticated users
- Enables user-level rate limiting
- Protects WeatherAPI.com quota

**Considerations:**
- Frontend must handle 401 responses
- Users must log in before viewing weather

---

#### 1.2 Restrict CORS to Your Domain
Only allow requests from your own website.

**Implementation:**
```c
static void add_cors_headers(struct MHD_Response *response) {
    if (server_config.cors_enabled) {
        // ONLY allow your domain
        MHD_add_response_header(response, "Access-Control-Allow-Origin", 
                                "https://weather.limani-parou.com");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", 
                                "GET, POST, PUT, DELETE, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", 
                                "Content-Type");
        MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
    }
}
```

**Benefits:**
- Browsers block cross-origin requests from other sites
- Simple one-line change
- No impact on legitimate users

**Limitations:**
- Only protects against browser-based scraping
- curl/wget/scripts can still bypass this
- Why authentication is still needed

---

#### 1.3 Basic Rate Limiting (In-Memory)
Limit requests per IP address.

**Implementation:**
```c
#define MAX_TRACKED_IPS 1000
#define RATE_LIMIT_WINDOW 60  // 1 minute
#define MAX_REQUESTS_PER_WINDOW 30  // 30 requests per minute

typedef struct {
    char ip_address[46];  // IPv6 max length
    time_t window_start;
    int request_count;
    int used;
} rate_limit_entry_t;

static rate_limit_entry_t rate_limits[MAX_TRACKED_IPS] = {0};

static bool check_rate_limit(const char *ip_address) {
    time_t now = time(NULL);
    
    // Find or create entry
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
    
    if (slot == -1) return true; // No slots available, allow (fail open)
    
    rate_limit_entry_t *entry = &rate_limits[slot];
    
    // Initialize new entry
    if (!entry->used) {
        strncpy(entry->ip_address, ip_address, 45);
        entry->window_start = now;
        entry->request_count = 1;
        entry->used = 1;
        return true;
    }
    
    // Check if window expired
    if (now - entry->window_start > RATE_LIMIT_WINDOW) {
        entry->window_start = now;
        entry->request_count = 1;
        return true;
    }
    
    // Check rate limit
    entry->request_count++;
    if (entry->request_count > MAX_REQUESTS_PER_WINDOW) {
        return false;  // Rate limited!
    }
    
    return true;
}

// In handle_weather_current():
const char *client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, 
                                                     MHD_HTTP_HEADER_X_FORWARDED_FOR);
if (!client_ip) {
    client_ip = "unknown";  // Could also get from connection struct
}

if (!check_rate_limit(client_ip)) {
    struct MHD_Response *response = create_error_response(429, "Too many requests");
    enum MHD_Result ret = MHD_queue_response(connection, 429, response);
    MHD_destroy_response(response);
    return ret;
}
```

**Benefits:**
- Prevents rapid-fire scraping
- Protects against DoS
- Simple in-memory implementation

**Considerations:**
- Resets on server restart
- Can block legitimate users behind NAT
- Should whitelist authenticated users

---

### Priority 2: Enhanced Protection

#### 2.1 User-Based Rate Limiting
More sophisticated: limit per authenticated user, not just IP.

```c
// Add to session structure:
typedef struct {
    // ... existing fields ...
    time_t rate_window_start;
    int rate_request_count;
} user_session_t;

bool check_user_rate_limit(user_session_t *session) {
    time_t now = time(NULL);
    
    if (now - session->rate_window_start > RATE_LIMIT_WINDOW) {
        session->rate_window_start = now;
        session->rate_request_count = 1;
        return true;
    }
    
    session->rate_request_count++;
    return session->rate_request_count <= MAX_REQUESTS_PER_USER;
}
```

---

#### 2.2 Referrer Header Validation
Check that requests come from your domain.

```c
static bool validate_referrer(struct MHD_Connection *connection) {
    const char *referer = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Referer");
    
    if (!referer) return false;  // No referer = suspicious
    
    // Check if referer starts with your domain
    if (strncmp(referer, "https://weather.limani-parou.com", 32) == 0) {
        return true;
    }
    
    return false;
}
```

**Limitations:**
- Referer header can be spoofed
- Some browsers/privacy tools strip referer
- Use as additional layer, not primary protection

---

#### 2.3 Request Origin Validation
Nginx/Ingress level protection.

In your Nginx Ingress configuration:
```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: weather-dashboard
  annotations:
    # Rate limiting at ingress level
    nginx.ingress.kubernetes.io/limit-rps: "10"
    nginx.ingress.kubernetes.io/limit-connections: "5"
    
    # Deny direct IP access
    nginx.ingress.kubernetes.io/server-snippet: |
      if ($host != "weather.limani-parou.com") {
        return 444;
      }
```

---

#### 2.4 API Key for External Access (Optional)
If you want to allow controlled external access:

```c
// Check for API key in header
const char *api_key = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-API-Key");

if (!session && (!api_key || !validate_api_key(api_key))) {
    // Neither session nor valid API key
    return 401;
}
```

---

### Priority 3: Monitoring & Detection

#### 3.1 Request Logging
Log all weather API requests for analysis:

```c
void log_weather_request(const char *ip, const char *user_id, 
                         const char *endpoint, bool allowed) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] Weather API: ip=%s user=%s endpoint=%s allowed=%d\n",
            timestamp, ip, user_id, endpoint, allowed);
}
```

#### 3.2 Anomaly Detection
Track unusual patterns:
- Same IP making many requests
- Same user from multiple IPs
- Requests outside normal hours
- Unusual location patterns

---

## Recommended Implementation Plan

### Phase 1 (Now - Critical)
1. **Require authentication** for `/api/weather/*` endpoints
2. **Restrict CORS** to `https://weather.limani-parou.com`
3. **Test** that legitimate users can still access

### Phase 2 (Next Sprint)
4. **Add basic IP-based rate limiting** (30 req/min)
5. **Add user-based rate limiting** (higher limit for authenticated users)
6. **Add request logging** for monitoring

### Phase 3 (Future)
7. **Implement Nginx-level rate limiting**
8. **Add anomaly detection alerts**
9. **Consider API keys** if external access needed

---

## Testing Checklist

After implementing protections:

- [ ] Logged-in users can access weather data
- [ ] Anonymous users get 401 error
- [ ] Requests from other domains blocked (CORS)
- [ ] Rate limit triggers after threshold
- [ ] Rate limit resets after time window
- [ ] Legitimate users not blocked
- [ ] Direct API calls (curl) blocked without session
- [ ] Frontend handles authentication errors gracefully

---

## Cost Analysis

**Before Protection:**
- Open API = Unlimited scraping
- WeatherAPI.com quota: Easy to exhaust
- Potential cost: $$ if quota exceeded

**After Protection:**
- Only authenticated users
- Rate limited to 30 req/min
- Max daily calls: ~43,000 per user
- WeatherAPI.com free tier: 1M calls/month = ~33,000/day
- **Protected!**

---

## Security Layers Summary

1. **CORS**: Blocks browser-based cross-origin requests
2. **Authentication**: Ensures only logged-in users access API
3. **Rate Limiting (IP)**: Prevents rapid scraping
4. **Rate Limiting (User)**: Prevents abuse by authenticated users
5. **Session Expiry**: Limits session lifetime (already implemented)
6. **HTTPS**: Prevents man-in-the-middle attacks
7. **Secure Cookies**: Prevents cookie theft (already implemented)

**Defense in Depth:** Even if one layer fails, others protect you.
