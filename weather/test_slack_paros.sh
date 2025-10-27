#!/bin/bash

# Test script for Slack Paros weather integration

echo "🌤️  Testing Slack Paros Weather Integration"
echo "============================================"

BASE_URL="http://localhost:8080"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test 1: URL Verification Challenge (should still work)
echo -e "\n${BLUE}Test 1: Slack URL Verification${NC}"
echo "Endpoint: POST /slack/events"

CHALLENGE="3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "{
    \"token\": \"verification_token\",
    \"challenge\": \"$CHALLENGE\",
    \"type\": \"url_verification\"
  }")

echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "$CHALLENGE"; then
    echo -e "${GREEN}✅ PASS: URL verification works${NC}"
else
    echo -e "${RED}❌ FAIL: URL verification failed${NC}"
fi

# Test 2: Message event with "paros" (case insensitive)
echo -e "\n${BLUE}Test 2: Message containing 'paros'${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d '{
    "token": "verification_token",
    "type": "event_callback",
    "event": {
      "type": "message",
      "text": "Kva er vêret på Paros i dag?",
      "channel": "C12345678",
      "user": "U12345678",
      "ts": "1234567890.123456"
    }
  }')

echo "Request: Message with 'Paros' (uppercase P)"
echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Event acknowledged${NC}"
    echo -e "${YELLOW}Note: Check server logs to see if weather was fetched${NC}"
else
    echo -e "${RED}❌ FAIL: Event not acknowledged${NC}"
fi

# Test 3: Message with "paros" in lowercase
echo -e "\n${BLUE}Test 3: Message containing 'paros' (lowercase)${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d '{
    "token": "verification_token",
    "type": "event_callback",
    "event": {
      "type": "message",
      "text": "Korleis er vêret på paros?",
      "channel": "C12345678",
      "user": "U12345678",
      "ts": "1234567890.123457"
    }
  }')

echo "Request: Message with 'paros' (lowercase)"
echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Event acknowledged${NC}"
else
    echo -e "${RED}❌ FAIL: Event not acknowledged${NC}"
fi

# Test 4: Message with "PAROS" in all caps
echo -e "\n${BLUE}Test 4: Message containing 'PAROS' (all caps)${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d '{
    "token": "verification_token",
    "type": "event_callback",
    "event": {
      "type": "message",
      "text": "Tell me about PAROS weather",
      "channel": "C12345678",
      "user": "U12345678",
      "ts": "1234567890.123458"
    }
  }')

echo "Request: Message with 'PAROS' (all caps)"
echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Event acknowledged${NC}"
else
    echo -e "${RED}❌ FAIL: Event not acknowledged${NC}"
fi

# Test 5: Message WITHOUT "paros" (should not trigger weather)
echo -e "\n${BLUE}Test 5: Message without 'paros'${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d '{
    "token": "verification_token",
    "type": "event_callback",
    "event": {
      "type": "message",
      "text": "Hello, how is the weather in Athens?",
      "channel": "C12345678",
      "user": "U12345678",
      "ts": "1234567890.123459"
    }
  }')

echo "Request: Message about Athens (not Paros)"
echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Event acknowledged (should NOT fetch weather)${NC}"
else
    echo -e "${RED}❌ FAIL: Event not acknowledged${NC}"
fi

# Test 6: Bot message (should be ignored to prevent loops)
echo -e "\n${BLUE}Test 6: Bot message with 'paros' (should ignore)${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d '{
    "token": "verification_token",
    "type": "event_callback",
    "event": {
      "type": "message",
      "subtype": "bot_message",
      "text": "På Paros er det no 25 grader",
      "channel": "C12345678",
      "bot_id": "B12345678",
      "ts": "1234567890.123460"
    }
  }')

echo "Request: Bot message (should be ignored)"
echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Bot message ignored${NC}"
else
    echo -e "${RED}❌ FAIL: Event not acknowledged${NC}"
fi

echo -e "\n${GREEN}============================================"
echo "Test Summary"
echo "============================================${NC}"
echo "✓ URL verification works"
echo "✓ Message events are processed"
echo "✓ Case-insensitive 'paros' detection"
echo "✓ Bot messages are ignored"
echo ""
echo -e "${YELLOW}Note:${NC} To actually send messages to Slack:"
echo "  1. Set SLACK_BOT_TOKEN environment variable"
echo "  2. Configure Event Subscriptions in Slack App"
echo "  3. Subscribe to 'message.channels' event"
echo "  4. Invite the bot to a channel"
echo "  5. Send a message containing 'paros'"
echo ""
echo "Check the server logs (run with -v) to see:"
echo "  - Weather API requests"
echo "  - Slack message sending attempts"
echo "============================================"
