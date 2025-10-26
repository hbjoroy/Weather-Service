#!/bin/bash

# Weather Service API Test Script
# This script tests all endpoints of the weather service web API

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVER_PORT=8080
SERVER_HOST="localhost"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"
TEST_LOCATION="London"
FORECAST_DAYS=3

echo -e "${BLUE}=== Weather Service API Test Script ===${NC}"
echo "Base URL: $BASE_URL"
echo

# Function to check if server is running
check_server() {
    if ! curl -s "$BASE_URL/health" > /dev/null 2>&1; then
        echo -e "${RED}Error: Weather service is not running on $BASE_URL${NC}"
        echo "Start the server with: ./build/weather_service -s -p $SERVER_PORT"
        exit 1
    fi
}

# Function to run a test
run_test() {
    local test_name="$1"
    local curl_command="$2"
    local expected_status="$3"
    
    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo "Command: $curl_command"
    
    # Execute the curl command and capture response
    response=$(eval "$curl_command" 2>&1)
    exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Response received${NC}"
        # Pretty print JSON if possible
        if command -v jq &> /dev/null; then
            echo "$response" | jq . 2>/dev/null || echo "$response"
        else
            echo "$response"
        fi
    else
        echo -e "${RED}✗ Request failed${NC}"
        echo "$response"
    fi
    echo
}

# Check if the server is running
check_server

echo -e "${BLUE}=== Testing Health Endpoint ===${NC}"
run_test "Health Check" \
    "curl -s '$BASE_URL/health'" \
    200

echo -e "${BLUE}=== Testing Current Weather Endpoints ===${NC}"

run_test "Current Weather (GET with location)" \
    "curl -s '$BASE_URL/current?location=$TEST_LOCATION'" \
    200

run_test "Current Weather (GET with location and AQI)" \
    "curl -s '$BASE_URL/current?location=$TEST_LOCATION&include_aqi=true'" \
    200

run_test "Current Weather (POST with JSON)" \
    "curl -s -X POST '$BASE_URL/current' -H 'Content-Type: application/json' -d '{\"location\": \"$TEST_LOCATION\"}'" \
    200

run_test "Current Weather (POST with JSON and AQI)" \
    "curl -s -X POST '$BASE_URL/current' -H 'Content-Type: application/json' -d '{\"location\": \"$TEST_LOCATION\", \"include_aqi\": true}'" \
    200

echo -e "${BLUE}=== Testing Forecast Endpoints ===${NC}"

run_test "Forecast (basic)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=$FORECAST_DAYS'" \
    200

run_test "Forecast (with AQI)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=$FORECAST_DAYS&include_aqi=true'" \
    200

run_test "Forecast (with alerts)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=$FORECAST_DAYS&include_alerts=true'" \
    200

run_test "Forecast (with hourly data)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=$FORECAST_DAYS&include_hourly=true'" \
    200

run_test "Forecast (with all options)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=$FORECAST_DAYS&include_aqi=true&include_alerts=true&include_hourly=true'" \
    200

echo -e "${BLUE}=== Testing Error Conditions ===${NC}"

run_test "Current Weather (missing location)" \
    "curl -s '$BASE_URL/current'" \
    400

run_test "Forecast (missing location)" \
    "curl -s '$BASE_URL/forecast?days=$FORECAST_DAYS'" \
    400

run_test "Forecast (missing days)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION'" \
    400

run_test "Forecast (invalid days - too low)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=0'" \
    400

run_test "Forecast (invalid days - too high)" \
    "curl -s '$BASE_URL/forecast?location=$TEST_LOCATION&days=20'" \
    400

run_test "Non-existent endpoint" \
    "curl -s '$BASE_URL/nonexistent'" \
    404

run_test "Invalid method on health endpoint" \
    "curl -s -X POST '$BASE_URL/health'" \
    405

echo -e "${BLUE}=== Testing CORS (if enabled) ===${NC}"

run_test "CORS preflight request" \
    "curl -s -X OPTIONS '$BASE_URL/current' -H 'Origin: http://example.com' -H 'Access-Control-Request-Method: GET'" \
    200

echo -e "${GREEN}=== All tests completed ===${NC}"
echo
echo -e "${BLUE}Available endpoints:${NC}"
echo "  GET  /health"
echo "  GET  /current?location=<location>&include_aqi=<true|false>"
echo "  POST /current (JSON body: {\"location\": \"...\", \"include_aqi\": true})"
echo "  GET  /forecast?location=<location>&days=<1-14>&include_aqi=<true|false>&include_alerts=<true|false>&include_hourly=<true|false>"
echo
echo -e "${BLUE}Example API calls:${NC}"
echo "curl '$BASE_URL/health'"
echo "curl '$BASE_URL/current?location=London'"
echo "curl -X POST '$BASE_URL/current' -H 'Content-Type: application/json' -d '{\"location\": \"New York\"}'"
echo "curl '$BASE_URL/forecast?location=Tokyo&days=5&include_hourly=true'"