#!/bin/bash

# Quick start script for testing Slack integration

echo "🌤️  Weather Service - Slack Integration Quick Start"
echo "===================================================="
echo ""

# Check if weather service is built
if [ ! -f "build/weather_service" ]; then
    echo "❌ Weather service not built. Building now..."
    make
    if [ $? -ne 0 ]; then
        echo "❌ Build failed. Please check the errors above."
        exit 1
    fi
fi

echo "Starting Weather Service on port 8080..."
echo "Press Ctrl+C to stop"
echo ""
echo "Available endpoints:"
echo "  - http://localhost:8080/health"
echo "  - http://localhost:8080/slack/events"
echo ""
echo "To test the Slack endpoint, run in another terminal:"
echo "  ./test_slack.sh"
echo ""
echo "For Slack integration with ngrok:"
echo "  1. In another terminal: ngrok http 8080"
echo "  2. Copy the ngrok HTTPS URL"
echo "  3. In Slack App settings, set Request URL to: https://YOUR_NGROK_ID.ngrok.io/slack/events"
echo "  4. Set SLACK_BOT_TOKEN and SLACK_APP_ID environment variables:"
echo "     export SLACK_BOT_TOKEN=xoxb-your-token"
echo "     export SLACK_APP_ID=A01234567"
echo ""
echo "See SLACK.md for detailed setup instructions"
echo ""
echo "Starting server..."
echo "=================================================="
echo ""

# Run the server in verbose mode (no API key needed for Slack endpoints)
./build/weather_service -s -p 8090 -v -C
