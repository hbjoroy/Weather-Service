# Authentik OIDC Integration

This guide explains how to integrate the Weather Dashboard with Authentik for OpenID Connect authentication.

## Prerequisites

- Running Authentik instance (e.g., `identity.limani-parou.com`)
- Admin access to Authentik
- Weather Dashboard deployed

## Authentik Configuration

### 1. Create OAuth2/OpenID Provider

1. Log into Authentik admin interface
2. Navigate to **Applications** → **Providers**
3. Click **Create** → **OAuth2/OpenID Provider**
4. Configure the provider:
   - **Name**: `limani-weather`
   - **Client type**: `Confidential`
   - **Client ID**: (auto-generated, copy this)
   - **Client Secret**: (auto-generated, copy this)
   - **Redirect URIs**: `https://weather.limani-parou.com/api/auth/callback`
   - **Scopes**: Select `openid`, `email`, `profile`
   - **Subject mode**: `Based on the User's hashed ID`
   - **Include claims in id_token**: `Yes` (recommended)
5. Click **Create**

### 2. Create Application

1. Navigate to **Applications** → **Applications**
2. Click **Create**
3. Configure the application:
   - **Name**: `Weather Dashboard`
   - **Slug**: `limani-weather`
   - **Provider**: Select the provider created above
   - **Launch URL**: `https://weather.limani-parou.com`
4. Click **Create**

### 3. Configure Access (Optional)

If you want to restrict access:

1. Navigate to **Applications** → **Applications**
2. Click on your application
3. Go to **Policy Bindings** tab
4. Add authorization policies as needed

## Weather Dashboard Configuration

### Environment Variables

Add these environment variables to your dashboard deployment:

```bash
# OIDC Configuration (endpoints auto-discovered from issuer)
OIDC_ISSUER="https://identity.limani-parou.com/application/o/limani-weather/"
OIDC_CLIENT_ID="<your-client-id-from-step-1>"
OIDC_CLIENT_SECRET="<your-client-secret-from-step-1>"
OIDC_REDIRECT_URI="https://weather.limani-parou.com/api/auth/callback"
```

**Note**: The backend automatically discovers authorization, token, userinfo, and end-session endpoints from the issuer's `.well-known/openid-configuration` endpoint. You only need to provide the issuer URL.

### Kubernetes Deployment

#### 1. Create Secret for OIDC Credentials

```bash
kubectl create secret generic oidc-credentials \
  --from-literal=client-id='<your-client-id>' \
  --from-literal=client-secret='<your-client-secret>' \
  -n weather
```

#### 2. Update values.yaml

```yaml
weatherDashboard:
  env:
    oidcIssuer: "https://identity.limani-parou.com/application/o/limani-weather/"
    oidcRedirectUri: "https://weather.limani-parou.com/api/auth/callback"
  
  existingOidcSecret: "oidc-credentials"
```

#### 3. Update Helm Deployment Template

The deployment template should inject OIDC configuration:

```yaml
env:
  - name: OIDC_ISSUER
    value: {{ .Values.weatherDashboard.env.oidcIssuer | quote }}
  - name: OIDC_CLIENT_ID
    valueFrom:
      secretKeyRef:
        name: {{ .Values.weatherDashboard.existingOidcSecret }}
        key: client-id
  - name: OIDC_CLIENT_SECRET
    valueFrom:
      secretKeyRef:
        name: {{ .Values.weatherDashboard.existingOidcSecret }}
        key: client-secret
  - name: OIDC_REDIRECT_URI
    value: {{ .Values.weatherDashboard.env.oidcRedirectUri | quote }}
```

#### 4. Deploy

```bash
helm upgrade weather-stack ./helm/weather-stack -n weather
```

## Authentication Flow

1. User clicks "Login" on the dashboard
2. Frontend calls `GET /api/auth/login`
3. Backend returns authorization URL
4. Frontend redirects user to Authentik
5. User authenticates with Authentik
6. Authentik redirects back to `/api/auth/callback?code=...&state=...`
7. Backend exchanges code for tokens
8. Backend creates session with user info
9. User is redirected to dashboard homepage

## Testing

### Local Testing

For local development without HTTPS:

1. Update Authentik redirect URIs to include `http://localhost:3001/api/auth/callback`
2. Set environment variables:
   ```bash
   export OIDC_ISSUER="https://identity.limani-parou.com/application/o/limani-weather/"
   export OIDC_CLIENT_ID="your-client-id"
   export OIDC_CLIENT_SECRET="your-client-secret"
   export OIDC_REDIRECT_URI="http://localhost:3001/api/auth/callback"
   
   ./build/weather-dashboard-server
   ```

3. Access `http://localhost:3001` and test login

### Production Testing

1. Verify HTTPS is working on your domain
2. Access `https://weather.limani-parou.com`
3. Click "Login"
4. Should redirect to Authentik
5. After authentication, should return to dashboard with profile loaded

## Troubleshooting

### "Invalid redirect_uri"

- Check that the redirect URI in Authentik exactly matches the one configured in the environment
- Ensure protocol (http/https) matches
- Check for trailing slashes

### "Invalid client credentials"

- Verify CLIENT_ID and CLIENT_SECRET are correct
- Check that the secret is properly created in Kubernetes
- Ensure no extra whitespace in credentials

### "State mismatch"

- This is a security error - the state parameter doesn't match
- Check server logs for details
- May indicate a replay attack or session timeout

### User info not loading

- Check that scopes include `openid`, `email`, `profile`
- Verify "Include claims in id_token" is enabled in Authentik
- Check backend logs for token exchange errors

## Security Considerations

1. **HTTPS Required**: OIDC should only be used over HTTPS in production
2. **Secure Secrets**: Store CLIENT_SECRET in Kubernetes secrets, not in code
3. **State Validation**: The backend validates CSRF state tokens
4. **Cookie Security**: Session cookies are HttpOnly and SameSite=Lax
5. **Token Storage**: ID tokens are not stored long-term, only used for logout

## Logout

The logout functionality will:
1. Destroy local session
2. (Future) Redirect to Authentik end-session endpoint to fully log out

To enable full logout:
- Store ID token in session
- Call `/api/logout` which will redirect to Authentik's end-session endpoint
- Authentik will invalidate the session and redirect back to the app

## User Profile Storage

- User ID from OIDC (`sub` claim) is used as the database key
- Profile data (preferences) are stored in PostgreSQL
- Name from OIDC is used as display name

## Migration from Fake Login

The backend maintains backwards compatibility:
- If OIDC is not configured, fake login still works (`POST /api/login`)
- Once OIDC is configured, use the new endpoints (`GET /api/auth/login`)
- Frontend should detect which method to use
