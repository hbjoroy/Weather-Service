# Paros Weather Slack Bot - Implementation Summary

## What Was Implemented

A Slack bot that automatically responds with Paros weather information whenever anyone mentions "paros" (case-insensitive) in a Slack channel.

## Features

### ✅ Automatic Message Detection
- Listens to all messages in channels where the bot is a member
- Case-insensitive detection of the word "paros"
- Works with any message containing "paros" anywhere in the text

### ✅ Weather Response
The bot responds in Norwegian with:
- Current temperature in Celsius
- Sky condition (e.g., "Sunny", "Partly cloudy")
- Wind speed in m/s (converted from km/h)
- Wind direction

**Example Response:**
```
På Paros er det no 22.5 grader og Sunny, vinden er 3.2 m/s, retning NW
```

### ✅ Smart Bot Detection
- Ignores messages from other bots (including itself) to prevent infinite loops
- Only responds to human messages

### ✅ Real-time Weather Data
- Fetches live weather data from WeatherAPI.com
- Location: "Paros, Greece"
- Automatic conversion of wind speed from km/h to m/s

## Files Modified/Created

### Core Implementation
1. **`src/http_server.c`**
   - Added `send_slack_message()` - Posts messages to Slack using chat.postMessage API
   - Added `contains_paros()` - Case-insensitive string search
   - Added `handle_paros_weather_request()` - Fetches weather and formats response
   - Updated `handle_slack_events()` - Processes message events from Slack
   - Added bot message filtering to prevent loops

2. **`src/http_client.c`**
   - Added `http_post_json()` - New function for POSTing JSON to APIs

3. **`include/http_client.h`**
   - Added declaration for `http_post_json()`

4. **`include/weather_types.h`**
   - Added `slack_bot_token` field to `server_config_t`

5. **`src/main.c`**
   - Added `-S/--slack` command line option
   - Added `SLACK_BOT_TOKEN` environment variable support
   - Updated help text with Slack examples

### Documentation & Testing
6. **`SLACK.md`** - Updated with:
   - Paros weather feature documentation
   - Setup instructions for Slack app
   - Event subscription requirements
   - Live testing examples

7. **`test_slack_paros.sh`** - Comprehensive test script:
   - Tests URL verification
   - Tests "paros" detection (uppercase, lowercase, mixed)
   - Tests bot message filtering
   - Tests non-paros messages

8. **`PAROS_DEMO.sh`** - Quick demo/info script

## How It Works

### 1. Message Flow
```
Slack User → Slack API → /slack/events → Weather Service
                                          ↓
                                    Detects "paros"
                                          ↓
                                    Fetch Weather API
                                          ↓
                                    Format Response
                                          ↓
Slack Channel ← Slack API ← Post Message
```

### 2. Message Processing
```c
// Simplified logic
if (event.type == "message" && !event.subtype == "bot_message") {
    if (contains_paros(event.text)) {
        weather = fetch_weather("Paros, Greece");
        message = format_norwegian_response(weather);
        send_slack_message(event.channel, message);
    }
}
```

### 3. Weather Data Conversion
- Wind speed: km/h → m/s (multiply by 0.277778)
- Temperature: Already in Celsius
- Condition: Direct from WeatherAPI

## Configuration Required

### Environment Variables
```bash
export WEATHERAPI_KEY=your_weatherapi_key        # Required
export SLACK_BOT_TOKEN=xoxb-your-slack-token    # Required for sending messages
```

### Slack App Configuration
1. **Event Subscriptions**: Enable and set URL to `https://your-server/slack/events`
2. **Bot Events**: Subscribe to:
   - `message.channels` (required)
   - `message.groups` (optional)
   - `message.im` (optional)
3. **OAuth Scopes**: Bot needs:
   - `chat:write` - To send messages
   - `channels:history` - To read messages in channels
4. **Installation**: Install app to workspace and get Bot Token

## Usage

### Starting the Server
```bash
# With environment variables
export WEATHERAPI_KEY=your_key
export SLACK_BOT_TOKEN=xoxb-your-token
./build/weather_service -s -p 8080 -v

# Or with command line args
./build/weather_service -s -p 8080 -v -S xoxb-your-token
```

### Testing Locally
```bash
# Terminal 1: Start server
./build/weather_service -s -p 8080 -v

# Terminal 2: Run tests
./test_slack_paros.sh

# Terminal 3: For internet access (ngrok)
ngrok http 8080
```

### Live Usage
1. Invite bot to a channel: `/invite @YourBotName`
2. Post any message with "paros":
   - "What's the weather in paros?"
   - "Kva er vêret på Paros?"
   - "Tell me about PAROS"
3. Bot responds immediately with current weather

## Technical Details

### API Calls
- **Incoming**: Slack Events API → `POST /slack/events`
- **Outgoing**: Weather Service → `GET https://api.weatherapi.com/v1/current.json`
- **Outgoing**: Weather Service → `POST https://slack.com/api/chat.postMessage`

### Response Time
- Slack expects acknowledgment within 3 seconds
- Weather fetch is synchronous but typically completes in <1 second
- Bot acknowledges immediately, then fetches/sends weather

### Error Handling
- Missing Slack token: Events acknowledged but no messages sent
- Weather API failure: Sends Norwegian error message to Slack
- Network issues: Logged to stderr with verbose mode

## Future Enhancements

Possible additions:
- Support for multiple locations
- Custom trigger words beyond "paros"
- Weather forecasts (not just current conditions)
- User preference storage
- Multilingual responses
- Scheduled daily weather updates
- Slash commands: `/weather paros`

## Testing

Run the test suite:
```bash
./test_slack_paros.sh
```

Expected output:
- ✅ URL verification works
- ✅ Messages with "paros" are detected (any case)
- ✅ Bot messages are ignored
- ✅ Events are acknowledged

## Security Notes

**Production Recommendations:**
1. Implement Slack request signature verification
2. Use HTTPS (required by Slack)
3. Store tokens securely (not in code)
4. Implement rate limiting
5. Add request logging for audit trail

**Current Security:**
- Tokens stored in environment variables
- Bot message filtering prevents loops
- All JSON parsing validated
- No token exposed in logs (with default settings)

## Troubleshooting

### "Beklager, kunne ikkje hente vêrdata"
- Check `WEATHERAPI_KEY` is set and valid
- Verify internet connection
- Check WeatherAPI.com quota

### No response in Slack
- Verify `SLACK_BOT_TOKEN` is set
- Check bot has `chat:write` scope
- Ensure bot is invited to the channel
- Check server logs with `-v` flag

### Bot not seeing messages
- Verify bot is invited to channel
- Check `message.channels` event subscription
- Verify Event URL is correct and verified
- Check bot has `channels:history` scope

## Links

- Main Documentation: `README.md`
- Slack Setup Guide: `SLACK.md`
- OpenAPI Spec: `openapi.yaml`
- Test Scripts: `test_slack.sh`, `test_slack_paros.sh`
