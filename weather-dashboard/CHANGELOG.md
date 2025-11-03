# Weather Dashboard Changelog

## Version 1.0.19 - 2025-11-03

### Security Enhancements - API Protection

**Critical security improvements to prevent API abuse and screen scraping:**

#### 1. Authentication Required for Weather Endpoints ✅
- **Feature:** All weather API endpoints (`/api/weather/current`, `/api/weather/forecast`) now require valid session authentication
- **Impact:** Prevents anonymous access and screen scraping
- **HTTP Response:** Returns 401 Unauthorized if not logged in
- **Files Changed:** `backend/src/http_server.c`

#### 2. CORS Restriction ✅
- **Before:** `Access-Control-Allow-Origin: *` (allowed all domains)
- **After:** `Access-Control-Allow-Origin: https://weather.limani-parou.com` (restricted to our domain)
- **Impact:** Browsers block cross-origin requests from other websites
- **Files Changed:** `backend/src/http_server.c`

#### 3. Rate Limiting ✅
- **Feature:** In-memory rate limiting by IP address
- **Limits:** 30 requests per 60-second window per IP
- **HTTP Response:** Returns 429 Too Many Requests when exceeded
- **Protection:** Prevents rapid-fire scraping and DoS attacks
- **Configuration:**
  - `MAX_REQUESTS_PER_WINDOW`: 30
  - `RATE_LIMIT_WINDOW`: 60 seconds
  - `MAX_TRACKED_IPS`: 1000 simultaneous IP addresses
- **Headers Used:** X-Forwarded-For, X-Real-IP (for IP detection behind proxies)
- **Files Changed:** `backend/src/http_server.c`

### Technical Details

**New Code Structures:**
```c
typedef struct {
    char ip_address[46];      // IPv6 max length
    time_t window_start;      // Start of current rate limit window
    int request_count;        // Requests in current window
    int used;                 // Slot in use flag
} rate_limit_entry_t;
```

**Function:** `check_rate_limit(const char *ip_address)`
- Tracks requests per IP in sliding 60-second windows
- Automatically resets counters after window expires
- Logs rate limit violations to stderr
- Fails open (allows request) if rate limit table is full

**Security Flow:**
1. Request arrives → Extract session cookie
2. Validate session → Return 401 if invalid/expired
3. Extract client IP → Check X-Forwarded-For, X-Real-IP headers
4. Check rate limit → Return 429 if exceeded
5. Process request → Return weather data

### Deployment Notes

- **Version:** 1.0.19
- **Deployed:** 2025-11-03 05:31:18 UTC
- **Revision:** 31
- **Rollout:** Clean deployment, all pods running with 0 restarts
- **Backward Compatibility:** Frontend changes required - must handle 401 responses

### Testing

**Anonymous Access Test:**
```bash
curl "https://weather.limani-parou.com/api/weather/current?location=Oslo"
# Expected: HTTP 401 Unauthorized
```

**CORS Header Test:**
```bash
curl -I "https://weather.limani-parou.com/api/weather/current?location=Oslo" | grep access-control-allow-origin
# Expected: access-control-allow-origin: https://weather.limani-parou.com
```

**Rate Limiting Test:**
```bash
for i in {1..35}; do 
  curl -s -o /dev/null -w "HTTP %{http_code}\n" \
    -H "Cookie: session_id=<valid_session>" \
    "https://weather.limani-parou.com/api/weather/current?location=Oslo"
done
# Expected: First 30 requests = 200 OK, Requests 31+ = 429 Too Many Requests
```

### Security Impact Analysis

**Before (v1.0.18):**
- ❌ Anyone could access weather API without authentication
- ❌ Any website could make cross-origin requests
- ❌ No protection against rapid scraping
- ❌ WeatherAPI.com quota vulnerable to exhaustion

**After (v1.0.19):**
- ✅ Only authenticated users can access weather data
- ✅ Only requests from weather.limani-parou.com allowed (browser-enforced)
- ✅ Maximum 30 requests per minute per IP address
- ✅ WeatherAPI.com quota protected from abuse
- ✅ Logging of rate limit violations for monitoring

### Known Limitations

1. **Rate limiting is in-memory:**
   - Resets on server restart
   - Not shared across multiple pods
   - Per-pod limit is 30 req/min (total capacity scales with pod count)

2. **IP-based rate limiting:**
   - Can affect legitimate users behind NAT/proxy
   - Doesn't distinguish between different authenticated users from same IP
   - Future enhancement: Per-user rate limiting in addition to per-IP

3. **CORS only protects browsers:**
   - Command-line tools (curl, wget) can bypass CORS
   - Authentication requirement is the primary defense

4. **No geographic restrictions:**
   - Future enhancement: IP geolocation checks if desired

### Future Enhancements (Considered)

- [ ] PostgreSQL-based rate limiting (persistent across restarts)
- [ ] Per-user rate limiting (higher limits for authenticated users)
- [ ] Redis-based distributed rate limiting (shared across pods)
- [ ] Request logging to database for audit trails
- [ ] Anomaly detection (unusual access patterns)
- [ ] API keys for controlled external access
- [ ] Nginx ingress-level rate limiting (additional layer)

### References

- Security documentation: `SECURITY_API_PROTECTION.md`
- OIDC authentication: `AUTHENTIK.md`
- Session management: `SECURITY_IMPROVEMENTS.md`

---

## Previous Versions

### Version 1.0.18 - 2025-11-02
- Fixed strdup implicit declaration with _GNU_SOURCE
- Renamed daemon variable to http_daemon to avoid conflicts
- Removed unused validate_state() function

### Version 1.0.17 - 2025-11-02
- Fixed base64url_encode buffer access after BIO_free_all

### Version 1.0.16 - 2025-11-02
- Implemented PKCE for OIDC authentication
- Added token refresh mechanism
- Session expiry (1 hour)
- Secure cookies with HttpOnly flag

### Version 1.0.15 and earlier
- See git history for details
