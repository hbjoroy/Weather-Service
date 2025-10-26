#!/bin/bash

# Example script showing how to use the weather service
# You can provide the API key in two ways:
# 1. Set WEATHERAPI_KEY environment variable
# 2. Use -k command line option

API_KEY="YOUR_API_KEY"
WEATHER_SERVICE="./build/weather_service"

echo "Weather Service Example Usage"
echo "============================="
echo
echo "Setup Options:"
echo "1. Get your free API key from https://www.weatherapi.com/"
echo "2. Choose one of these methods to provide your API key:"
echo "   Method A: Set environment variable:"
echo "     export WEATHERAPI_KEY=your_actual_api_key"
echo "   Method B: Use command line option -k your_actual_api_key"
echo "3. Build the project with 'make'"
echo
echo "Example commands:"
echo

echo "# Method A: Using environment variable (recommended)"
echo "export WEATHERAPI_KEY=your_actual_api_key"
echo "$WEATHER_SERVICE \"London\""
echo "$WEATHER_SERVICE -a \"New York\"  # With air quality data"
echo "$WEATHER_SERVICE -f5 \"Tokyo\"     # 5-day forecast"
echo "$WEATHER_SERVICE -f3 -H \"Paris\"  # 3-day forecast with hourly details"
echo "$WEATHER_SERVICE \"Paros\""
echo
echo "# Method B: Using command line option"
echo "$WEATHER_SERVICE -k your_actual_api_key \"London\""
echo "$WEATHER_SERVICE -k your_actual_api_key -a \"New York\""
echo "$WEATHER_SERVICE -k your_actual_api_key -f7 \"Tokyo\""
echo "$WEATHER_SERVICE -k your_actual_api_key -f5 -H -a \"Berlin\""
echo "$WEATHER_SERVICE -k your_actual_api_key \"Paros\""
echo

echo "# Other location formats and options:"
echo "$WEATHER_SERVICE \"37.7749,-122.4194\"  # Coordinates (San Francisco)"
echo "$WEATHER_SERVICE \"auto:ip\"             # Current location by IP"
echo "$WEATHER_SERVICE -f10 -H \"London\"      # 10-day forecast with hourly data"
echo "$WEATHER_SERVICE -f14 -a -A \"Miami\"    # 14-day forecast with AQI and alerts"
echo

# Check if environment variable is set
if [ -n "$WEATHERAPI_KEY" ] && [ "$WEATHERAPI_KEY" != "YOUR_API_KEY" ]; then
    echo "🚀 Found WEATHERAPI_KEY environment variable! Running examples..."
    echo
    echo "Current weather for Paros:"
    $WEATHER_SERVICE "Paros"
    echo
    echo "3-day forecast for London:"
    $WEATHER_SERVICE -f3 "London"
elif [ "$API_KEY" != "YOUR_API_KEY" ]; then
    echo "🚀 Running example with API key from script variable..."
    echo
    echo "Current weather for Paros:"
    $WEATHER_SERVICE -k "$API_KEY" "Paros"
    echo
    echo "3-day forecast for London:"
    $WEATHER_SERVICE -k "$API_KEY" -f3 "London"
else
    echo "💡 To run examples:"
    echo "   Option 1: export WEATHERAPI_KEY=your_actual_api_key"
    echo "   Option 2: Edit this script and replace YOUR_API_KEY with your actual key"
fi