#!/bin/bash

# Test script for Slack integration endpoints

echo "🔧 Testing Slack Integration Endpoints"
echo "========================================"

BASE_URL="http://localhost:8080"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test 1: URL Verification Challenge
echo -e "\n${BLUE}Test 1: Slack URL Verification${NC}"
echo "Endpoint: POST /slack/events"

CHALLENGE="3eZbrw1aBm2rZgRNFdxV2595E9CY3gmdALWMmHkvFXO7tYXAYM8P"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "{
    \"token\": \"Jhj5dZrVaK7ZwHHjRyZWjbDl\",
    \"challenge\": \"$CHALLENGE\",
    \"type\": \"url_verification\"
  }")

echo "Request:"
echo "{
  \"token\": \"Jhj5dZrVaK7ZwHHjRyZWjbDl\",
  \"challenge\": \"$CHALLENGE\",
  \"type\": \"url_verification\"
}"

echo -e "\nResponse:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

# Check if response contains the challenge
if echo "$RESPONSE" | grep -q "$CHALLENGE"; then
    echo -e "${GREEN}✅ PASS: Challenge echoed back correctly${NC}"
else
    echo -e "${RED}❌ FAIL: Challenge not found in response${NC}"
fi

# Test 2: Invalid JSON
echo -e "\n${BLUE}Test 2: Invalid JSON${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "invalid json")

echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "Invalid JSON"; then
    echo -e "${GREEN}✅ PASS: Invalid JSON handled correctly${NC}"
else
    echo -e "${YELLOW}⚠️  WARNING: Expected 'Invalid JSON' error${NC}"
fi

# Test 3: Missing type field
echo -e "\n${BLUE}Test 3: Missing 'type' field${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "{\"challenge\": \"test\"}")

echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "Missing 'type' field"; then
    echo -e "${GREEN}✅ PASS: Missing type field handled correctly${NC}"
else
    echo -e "${YELLOW}⚠️  WARNING: Expected 'Missing type field' error${NC}"
fi

# Test 4: Missing challenge in url_verification
echo -e "\n${BLUE}Test 4: Missing 'challenge' in url_verification${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "{\"type\": \"url_verification\"}")

echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "Missing 'challenge' field"; then
    echo -e "${GREEN}✅ PASS: Missing challenge field handled correctly${NC}"
else
    echo -e "${YELLOW}⚠️  WARNING: Expected 'Missing challenge field' error${NC}"
fi

# Test 5: Other event type (for future implementation)
echo -e "\n${BLUE}Test 5: Other Slack event type${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/slack/events" \
  -H "Content-Type: application/json" \
  -d "{
    \"token\": \"verification_token\",
    \"type\": \"event_callback\",
    \"event\": {
      \"type\": \"app_mention\",
      \"text\": \"Hello bot!\"
    }
  }")

echo "Response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

if echo "$RESPONSE" | grep -q "ok"; then
    echo -e "${GREEN}✅ PASS: Event acknowledged${NC}"
else
    echo -e "${YELLOW}⚠️  WARNING: Event not acknowledged properly${NC}"
fi

echo -e "\n${GREEN}========================================"
echo "Slack Integration Tests Complete"
echo -e "========================================${NC}"
