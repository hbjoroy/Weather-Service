# Slack Integration

This document describes how to integrate the Weather Service with Slack using the Slack Events API.

## Overview

The Weather Service provides a `/slack/events` endpoint that can receive events from Slack. This allows you to build Slack apps that interact with weather data.

**Current Features:**
- ✅ URL verification for Slack app setup
- ✅ Automatic weather responses when "paros" is mentioned in any message
- 🔜 Future: Custom slash commands, interactive messages, and more

## Paros Weather Bot

The service includes a built-in feature that automatically responds with Paros weather when anyone mentions "paros" (case-insensitive) in a message.

**Example:**
- User posts: "Kva er vêret på Paros i dag?"
- Bot responds: "På Paros er det no 22.5 grader og Sunny, vinden er 3.2 m/s, retning NW"

The response is in Norwegian and includes:
- Current temperature (in Celsius)
- Sky conditions
- Wind speed (converted from km/h to m/s)
- Wind direction

## Endpoints

### POST /slack/events

Handles incoming events from Slack's Events API.

**Supported Event Types:**
- `url_verification` - URL verification challenge (required for Slack app setup)
- `event_callback` - General event callbacks (for future event handling)

## Setting Up Slack Integration

### 1. Create a Slack App

1. Go to [Slack API Applications](https://api.slack.com/apps)
2. Click "Create New App"
3. Choose "From scratch"
4. Give your app a name (e.g., "Weather Bot")
5. Select your workspace

### 2. Enable Event Subscriptions

1. In your Slack app settings, go to "Event Subscriptions"
2. Toggle "Enable Events" to ON
3. Set the Request URL to: `http://your-server:8080/slack/events`
   - For development with ngrok: `https://your-ngrok-id.ngrok.io/slack/events`
   - Slack will send a verification challenge to this URL
   - The weather service will automatically respond with the challenge

### 3. URL Verification

When you enter the Request URL, Slack will send a verification challenge:

```json
{
  "token": "Jhj5dZrVaK7ZwHHjRyZWjbDl",
  "challenge": "3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P",
  "type": "url_verification"
}
```

The weather service automatically responds with:

```json
{
  "challenge": "3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P"
}
```

### 4. Subscribe to Bot Events (Optional - for future use)

Once verification is complete, you MUST subscribe to message events for the Paros weather feature to work:

**Required Events:**
- `message.channels` - Messages in public channels where the bot is a member

**Recommended Events:**
- `message.groups` - Messages in private channels
- `message.im` - Direct messages to the bot
- `app_mention` - When the bot is @mentioned

After subscribing to events:
1. Click "Install App" or "Reinstall App" 
2. Copy the "Bot User OAuth Token" (starts with `xoxb-`)
3. Find your App ID in "Basic Information" section (e.g., `A01234567`)
4. Set both as environment variables or pass them to the server:
   ```bash
   export SLACK_BOT_TOKEN=xoxb-your-token-here
   export SLACK_APP_ID=A01234567
   ```

**Important:** Setting `SLACK_APP_ID` prevents duplicate messages by ignoring messages that come from your own bot.

### 5. Invite the Bot to Channels

For the bot to see messages:
1. Open a Slack channel
2. Type: `/invite @YourBotName`
3. The bot can now see and respond to messages in that channel

## Running the Server with Slack Integration

Start the server with your Slack bot token:

```bash
export WEATHERAPI_KEY=your_weatherapi_key
export SLACK_BOT_TOKEN=xoxb-your-slack-bot-token
export SLACK_APP_ID=A01234567
./build/weather_service -s -p 8080 -v
```

Or pass the tokens as command-line arguments:

```bash
./build/weather_service -s -p 8080 -v -S xoxb-your-slack-bot-token -I A01234567
```

**Important:** The bot needs these values:
- `WEATHERAPI_KEY` - To fetch weather data from WeatherAPI.com
- `SLACK_BOT_TOKEN` - To send messages back to Slack
- `SLACK_APP_ID` - (Recommended) To prevent duplicate messages by ignoring own messages

## Testing Locally with ngrok

Since Slack needs to reach your service over the internet, you can use ngrok for local development:

```bash
# Start the weather service
cd /path/to/weather
export WEATHERAPI_KEY=your_api_key
./build/weather_service -s -p 8080 -v

# In another terminal, start ngrok
ngrok http 8080
```

Use the ngrok HTTPS URL (e.g., `https://abc123.ngrok.io/slack/events`) as your Slack Request URL.

## Testing the Endpoint

You can test the endpoint locally using the provided test scripts:

```bash
# Start the weather service in one terminal
export WEATHERAPI_KEY=your_api_key
export SLACK_BOT_TOKEN=xoxb-your-token  # Optional, for testing message sending
export SLACK_APP_ID=A01234567           # Recommended, prevents duplicates
./build/weather_service -s -p 8080 -v

# In another terminal, run the test scripts
./test_slack.sh          # Test URL verification
./test_slack_paros.sh    # Test Paros weather feature
```

**Note:** Without a valid `SLACK_BOT_TOKEN`, the server will:
- ✅ Accept and acknowledge all events
- ✅ Detect "paros" in messages
- ✅ Fetch weather data from WeatherAPI
- ❌ Fail to send the response back to Slack (you'll see an error in logs)

## Live Testing with a Real Slack Workspace

Once configured:

1. Invite the bot to a channel: `/invite @YourBotName`
2. Send any message containing "paros" (case doesn't matter):
   - "What's the weather in paros?"
   - "Kva er vêret på Paros?"
   - "PAROS weather please"
3. The bot will respond with current Paros weather in Norwegian

Example interaction:
```
User: Korleis er vêret på Paros i dag?
Bot:  På Paros er det no 22.5 grader og Sunny, vinden er 3.2 m/s, retning NW
```

Or test manually with curl:

```bash
# Test URL verification
curl -X POST http://localhost:8080/slack/events \
  -H "Content-Type: application/json" \
  -d '{
    "token": "Jhj5dZrVaK7ZwHHjRyZWjbDl",
    "challenge": "test_challenge_string",
    "type": "url_verification"
  }'

# Expected response:
# {"challenge": "test_challenge_string"}
```

## Verbose Logging

Enable verbose mode to see detailed logging of Slack events:

```bash
./build/weather_service -s -p 8080 -v
```

You'll see output like:
```
POST /slack/events - Body: {"token":"...","challenge":"...","type":"url_verification"}
Slack URL verification - challenge: 3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P
```

## Error Handling

The endpoint handles various error cases:

| Error | Response | Description |
|-------|----------|-------------|
| Invalid JSON | `400 Bad Request` | Request body is not valid JSON |
| Missing `type` field | `400 Bad Request` | The `type` field is required |
| Missing `challenge` | `400 Bad Request` | URL verification requires `challenge` field |
| Empty request | `400 Bad Request` | Request body is empty |
| Memory error | `500 Internal Server Error` | Server memory allocation failed |

## Future Enhancements

The current implementation supports URL verification and automatic Paros weather responses. Future versions may include:

- **Custom Location Queries**: Support for asking about any location
  - Example: "@weatherbot what's the weather in London?"
- **Slash Commands**: Dedicated weather slash commands
  - Example: `/weather London`
- **Weather Forecasts**: Multi-day forecasts in addition to current conditions
- **Interactive Messages**: Weather forecasts with interactive buttons
- **Scheduled Messages**: Daily weather briefings for Paros
- **User Preferences**: Remember preferred locations and temperature units
- **Direct Messages**: Private weather conversations
- **Multiple Languages**: Support for English, Norwegian, and Greek responses

## Security Considerations

For production use, you should:

1. **Verify Slack Requests**: Implement request signature verification
   - Slack signs all requests with a timestamp and signature
   - See: [Verifying requests from Slack](https://api.slack.com/authentication/verifying-requests-from-slack)

2. **Use HTTPS**: Always use HTTPS in production
   - Slack requires HTTPS for event URLs
   - Use a reverse proxy (nginx, Apache) or load balancer

3. **Rate Limiting**: Implement rate limiting to prevent abuse

4. **Token Validation**: Validate the verification token (or better, use signing secrets)

## Troubleshooting

### Slack shows "Your URL didn't respond with the challenge"

- Check that your server is running: `curl http://localhost:8080/health`
- Verify ngrok is forwarding correctly
- Check server logs with verbose mode (`-v`)
- Ensure no firewall is blocking the connection

### Getting 404 errors

- Verify the endpoint URL is exactly `/slack/events` (no trailing slash)
- Check server logs to see what URL is being requested

### Getting timeout errors

- Slack expects a response within 3 seconds
- The weather service responds immediately for URL verification
- Check server performance and network latency

## API Reference

See `openapi.yaml` for the complete API specification of the `/slack/events` endpoint.

## Examples

### URL Verification

Request:
```json
POST /slack/events
Content-Type: application/json

{
  "token": "Jhj5dZrVaK7ZwHHjRyZWjbDl",
  "challenge": "3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P",
  "type": "url_verification"
}
```

Response:
```json
HTTP/1.1 200 OK
Content-Type: application/json

{
  "challenge": "3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P"
}
```

### Message Event with "Paros"

Request:
```json
POST /slack/events
Content-Type: application/json

{
  "token": "verification_token",
  "type": "event_callback",
  "event": {
    "type": "message",
    "text": "Kva er vêret på Paros?",
    "channel": "C12345678",
    "user": "U12345678",
    "ts": "1234567890.123456"
  }
}
```

Response (to acknowledge the event):
```json
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "ok"
}
```

Bot's message posted to Slack (asynchronously):
```
På Paros er det no 22.5 grader og Sunny, vinden er 3.2 m/s, retning NW
```

### Event Callback (Acknowledged)

Request:
```json
POST /slack/events
Content-Type: application/json

{
  "token": "verification_token",
  "type": "event_callback",
  "event": {
    "type": "app_mention",
    "text": "Hello weather bot!"
  }
}
```

Response:
```json
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "ok"
}
```
