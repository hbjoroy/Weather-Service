# Weather Service - C API Client & HTTP Server

A robust C application that fetches weather data from WeatherAPI.com and serves it to other applications. This project demonstrates modern C development practices including HTTP client implementation, JSON parsing, HTTP server functionality, and proper error handling.

## Features

- **HTTP Client**: Built on libcurl for reliable web service communication
- **HTTP Server**: Built-in web server mode using libmicrohttpd for serving JSON APIs
- **JSON Parsing**: Uses cJSON for parsing WeatherAPI responses and generating API responses
- **Comprehensive Data**: Supports all WeatherAPI.com current weather and forecast fields
- **Weather Forecasts**: Support for 1-14 day weather forecasts with hourly details
- **Flexible Location Input**: Supports city names, coordinates, and IP-based location
- **REST API**: Complete REST API endpoints with OpenAPI 3.0 documentation
- **Error Handling**: Robust error handling for network, API, and server failures
- **Command Line Interface**: Easy-to-use CLI with helpful options
- **Server Mode**: Run as a web service to serve weather data to other applications
- **Cross-Platform**: Works on Linux, macOS, and Windows

## Project Structure

```
weather/
├── src/                    # Source files
│   ├── main.c             # Main application entry point
│   ├── weather_api.c      # Weather API client implementation
│   ├── http_client.c      # HTTP client using libcurl
│   └── http_server.c      # HTTP server implementation using libmicrohttpd
├── include/               # Header files
│   ├── weather_types.h    # Data structure definitions
│   ├── weather_api.h      # Weather API interface
│   ├── http_client.h      # HTTP client interface
│   └── http_server.h      # HTTP server interface
├── build/                 # Build artifacts (generated)
├── lib/                   # External libraries (if needed)
├── openapi.yaml          # OpenAPI 3.0 specification for the web service
├── test_api.sh           # Test script for web service endpoints
├── example.sh            # Usage examples for CLI mode
├── Makefile              # Build configuration
└── README.md             # This file
```

## Dependencies

This project requires the following libraries:

- **libcurl**: For HTTP requests
- **cJSON**: For JSON parsing and generation
- **libmicrohttpd**: For HTTP server functionality (web service mode)

### Installing Dependencies

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev libcjson-dev libmicrohttpd-dev
```

#### CentOS/RHEL/Fedora:
```bash
sudo dnf install -y libcurl-devel cjson-devel libmicrohttpd-devel
```

#### macOS (with Homebrew):
```bash
brew install curl cjson libmicrohttpd
```

#### Or use the provided Makefile targets:
```bash
make deps        # For Debian/Ubuntu
make deps-rpm    # For CentOS/RHEL/Fedora
```

## Building

### Quick Build
```bash
make
```

### Clean and Rebuild
```bash
make clean
make
```

### Debug Build
```bash
make debug
```

### Available Make Targets
- `make` or `make all` - Build the weather service
- `make clean` - Remove build artifacts
- `make deps` - Install dependencies (Debian/Ubuntu)
- `make deps-rpm` - Install dependencies (CentOS/RHEL/Fedora)
- `make run` - Build and run the program
- `make debug` - Build debug version
- `make help` - Show available targets

## Usage

### Get Your API Key

1. Sign up for a free account at [WeatherAPI.com](https://www.weatherapi.com/)
2. Get your API key from the dashboard
3. The free tier includes 1 million calls per month

### API Key Configuration

You can provide your API key in two ways:

**Option 1: Environment Variable (Recommended)**
```bash
export WEATHERAPI_KEY=your_actual_api_key
./build/weather_service "London"
```

**Option 2: Command Line Option**
```bash
./build/weather_service -k your_actual_api_key "London"
```

*Note: Command line option takes precedence over environment variable.*

### Basic Usage

```bash
# Build the project
make

# Option 1: Using environment variable (recommended)
export WEATHERAPI_KEY=your_actual_api_key
./build/weather_service "London"

# Option 2: Using command line option
./build/weather_service -k your_actual_api_key "London"
```

### Command Line Options

```
Usage: weather_service [OPTIONS] <location>

CLI Mode Options:
  -k, --key <API_KEY>     WeatherAPI.com API key
  -a, --aqi               Include air quality data
  -f<n>                   Get forecast for n days (1-14), e.g., -f5 for 5-day forecast
  -u, --url <URL>         Base API URL (default: https://api.weatherapi.com/v1)
  -t, --timeout <SEC>     Request timeout in seconds (default: 30)
  -h, --help              Show help message

Server Mode Options:
  -s, --server            Run in server mode (HTTP web service)
  -p, --port <PORT>       Server port (default: 8080)
  -b, --bind <ADDRESS>    Bind address (default: 0.0.0.0)
  -v, --verbose           Enable verbose logging
  -c, --cors              Enable CORS headers

API KEY:
  The API key can be provided in two ways:
  1. Command line option: -k YOUR_API_KEY
  2. Environment variable: export WEATHERAPI_KEY=YOUR_API_KEY
  Command line option takes precedence over environment variable.

CLI EXAMPLES:
  # Using environment variable:
  export WEATHERAPI_KEY=YOUR_API_KEY
  weather_service "London"
  weather_service -a "New York"
  weather_service -f5 "Tokyo"  # 5-day forecast
  
  # Using command line option:
  weather_service -k YOUR_API_KEY "London"
  weather_service -k YOUR_API_KEY -a "New York"
  weather_service -k YOUR_API_KEY -f3 "Paris"

SERVER EXAMPLES:
  # Start server on default port 8080
  export WEATHERAPI_KEY=YOUR_API_KEY
  weather_service -s
  
  # Start server on custom port with CORS
  weather_service -s -p 3000 -c -v
```

## HTTP Web Service Mode

The weather service can run as an HTTP server, providing RESTful JSON endpoints for fetching weather data. This allows you to integrate weather data into web applications, mobile apps, or other services.

### Starting the Server

```bash
# Set your API key
export WEATHERAPI_KEY=your_actual_api_key

# Start server on default port 8080
./build/weather_service -s

# Start server on custom port
./build/weather_service -s -p 3000

# Start with verbose logging and CORS enabled
./build/weather_service -s -p 8080 -v -c
```

### Available Endpoints

#### Health Check
```http
GET /health
```
Returns server health status.

**Response:**
```json
{
  "status": "healthy",
  "service": "weather-api",
  "version": "1.0.0"
}
```

#### Current Weather (GET)
```http
GET /current?location=<location>&include_aqi=<true|false>
```

**Parameters:**
- `location` (required): Location query (city, coordinates, etc.)
- `include_aqi` (optional): Include air quality data (default: false)

**Example:**
```bash
curl "http://localhost:8080/current?location=London&include_aqi=true"
```

#### Current Weather (POST)
```http
POST /current
Content-Type: application/json

{
  "location": "London",
  "include_aqi": true
}
```

**Example:**
```bash
curl -X POST http://localhost:8080/current \
  -H "Content-Type: application/json" \
  -d '{"location": "New York", "include_aqi": true}'
```

#### Weather Forecast
```http
GET /forecast?location=<location>&days=<1-14>&include_aqi=<true|false>&include_alerts=<true|false>&include_hourly=<true|false>
```

**Parameters:**
- `location` (required): Location query
- `days` (required): Number of forecast days (1-14)
- `include_aqi` (optional): Include air quality data (default: false)
- `include_alerts` (optional): Include weather alerts (default: false)
- `include_hourly` (optional): Include hourly forecast data (default: false)

**Example:**
```bash
curl "http://localhost:8080/forecast?location=Tokyo&days=5&include_hourly=true"
```

### Web Service Examples

```bash
# Health check
curl http://localhost:8080/health

# Current weather for London
curl "http://localhost:8080/current?location=London"

# Current weather with air quality
curl "http://localhost:8080/current?location=New York&include_aqi=true"

# POST request for current weather
curl -X POST http://localhost:8080/current \
  -H "Content-Type: application/json" \
  -d '{"location": "Paris", "include_aqi": true}'

# 3-day forecast
curl "http://localhost:8080/forecast?location=Tokyo&days=3"

# 7-day forecast with hourly data and alerts
curl "http://localhost:8080/forecast?location=Berlin&days=7&include_hourly=true&include_alerts=true"
```

### Error Responses

All endpoints return standardized error responses:

```json
{
  "error": {
    "code": 400,
    "message": "Missing 'location' parameter",
    "details": "The location parameter is required for all weather requests"
  }
}
```

### API Testing

Use the provided test script to verify all endpoints:

```bash
# Start the server
export WEATHERAPI_KEY=your_actual_api_key
./build/weather_service -s &

# Run the test script
./test_api.sh
```

### OpenAPI Documentation

Complete OpenAPI 3.0 specification is available in `openapi.yaml`. You can view it using:

- **Swagger UI**: Load the file at https://editor.swagger.io/
- **Redoc**: Use any OpenAPI documentation viewer
- **Command line**: Use tools like `swagger-codegen` or `openapi-generator`

### Integration Examples

#### JavaScript/Node.js
```javascript
// Fetch current weather
const response = await fetch('http://localhost:8080/current?location=London');
const weather = await response.json();
console.log(`Temperature: ${weather.current.temp_c}°C`);

// Post request for weather data
const response = await fetch('http://localhost:8080/current', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ location: 'New York', include_aqi: true })
});
const weather = await response.json();
```

#### Python
```python
import requests

# Get current weather
response = requests.get('http://localhost:8080/current', 
                       params={'location': 'London', 'include_aqi': True})
weather = response.json()
print(f"Temperature: {weather['current']['temp_c']}°C")

# Get forecast
response = requests.get('http://localhost:8080/forecast',
                       params={'location': 'Tokyo', 'days': 5, 'include_hourly': True})
forecast = response.json()
```

#### curl
```bash
# Basic current weather
curl "http://localhost:8080/current?location=London"

# Forecast with all options
curl "http://localhost:8080/forecast?location=Berlin&days=7&include_aqi=true&include_alerts=true&include_hourly=true"
```

### Command Line Options (Legacy)

```
Usage: weather_service [OPTIONS] <location>

OPTIONS:
  -k, --key <API_KEY>     WeatherAPI.com API key
  -a, --aqi               Include air quality data
  -u, --url <URL>         Base API URL (default: https://api.weatherapi.com/v1)
  -t, --timeout <SEC>     Request timeout in seconds (default: 30)
  -h, --help              Show help message

API KEY:
  The API key can be provided in two ways:
  1. Command line option: -k YOUR_API_KEY
  2. Environment variable: export WEATHERAPI_KEY=YOUR_API_KEY
  Command line option takes precedence over environment variable.

EXAMPLES:
  # Using environment variable:
  export WEATHERAPI_KEY=YOUR_API_KEY
  weather_service "London"
  weather_service -a "New York"
  
  # Using command line option:
  weather_service -k YOUR_API_KEY "London"
  weather_service -k YOUR_API_KEY -a "New York"
```

### Location Formats

The weather service supports various location input formats:

- **City name**: `"London"`, `"New York"`
- **City with state/country**: `"London, UK"`, `"New York, NY"`
- **Coordinates**: `"37.7749,-122.4194"` (latitude,longitude)
- **IP-based location**: `"auto:ip"` (uses your current IP location)

### Example Output

```
Weather Service Starting...
Location: Paros
Include AQI: No
API Base URL: https://api.weatherapi.com/v1
Timeout: 30 seconds

Fetching weather data...

=== Weather Information ===
Location: Paros, South Aegean, Greece
Coordinates: 37.0833, 25.1500
Local Time: 2025-10-26 14:27
Timezone: Europe/Athens

=== Current Conditions ===
Condition: Sunny
Temperature: 23.4°C (74.1°F)
Feels Like: 25.2°C (77.4°F)
Humidity: 65%
Wind: 16.9 kph (10.5 mph) SW
Pressure: 1013.0 mb (29.91 in)
Visibility: 10.0 km (6.0 miles)
UV Index: 3.2
Cloud Cover: 0%
Last Updated: 2025-10-26 14:15

Weather data fetched successfully!
```

## API Integration

### Using the Library in Your Code

You can integrate the weather service into your own C applications:

```c
#include "weather_api.h"

int main() {
    // Configure the API
    weather_config_t config;
    strcpy(config.api_key, "your_api_key_here");
    strcpy(config.base_url, "https://api.weatherapi.com/v1");
    config.timeout = 30;
    
    // Initialize
    if (weather_api_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize weather API\n");
        return -1;
    }
    
    // Fetch weather data
    weather_response_t response;
    if (weather_api_get_current("London", 0, &response) == 0) {
        // Use the weather data
        printf("Temperature: %.1f°C\n", response.current.temp_c);
        printf("Condition: %s\n", response.current.condition.text);
        
        // Clean up
        weather_response_free(&response);
    }
    
    // Cleanup
    weather_api_cleanup();
    return 0;
}
```

### Data Structures

The main data structures are defined in `weather_types.h`:

- `weather_response_t`: Complete weather response
- `location_t`: Location information
- `current_weather_t`: Current weather data
- `weather_condition_t`: Weather condition details

## Error Handling

The application includes comprehensive error handling for:

- **Network failures**: Connection timeouts, DNS resolution failures
- **API errors**: Invalid API keys, rate limiting, malformed requests
- **JSON parsing errors**: Malformed responses, missing fields
- **Memory allocation failures**: Out of memory conditions
- **Invalid input**: Empty locations, invalid coordinates

## Development

### Code Style

- Follow ANSI C99 standard
- Use descriptive variable and function names
- Include comprehensive comments
- Handle all error conditions
- Free allocated memory properly

### Adding Features

To extend the weather service:

1. Add new data structures to `weather_types.h`
2. Implement parsing in `weather_api.c`
3. Update display functions as needed
4. Add command line options in `main.c`

### Testing

Test the application with various inputs:

```bash
# Valid city names
./build/weather_service -k YOUR_KEY "London"
./build/weather_service -k YOUR_KEY "New York, NY"

# Coordinates
./build/weather_service -k YOUR_KEY "40.7128,-74.0060"

# Current location
./build/weather_service -k YOUR_KEY "auto:ip"

# Error conditions
./build/weather_service -k invalid_key "London"
./build/weather_service -k YOUR_KEY "NonexistentCity12345"
```

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Make sure libcurl and cjson development packages are installed
   - Check that your compiler supports C99

2. **API Key Errors**
   - Verify your API key is correct
   - Check you haven't exceeded the rate limit
   - Ensure the API key has the necessary permissions

3. **Network Issues**
   - Check your internet connection
   - Verify firewall settings allow outbound HTTPS
   - Try increasing the timeout with `-t` option

4. **Location Not Found**
   - Check spelling of location name
   - Try using coordinates instead
   - Use more specific location (include state/country)

### Debug Mode

Build with debug information for troubleshooting:

```bash
make debug
gdb ./build/weather_service
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is open source. Feel free to use, modify, and distribute according to your needs.

## Support

For issues and questions:
- Check the troubleshooting section above
- Review the WeatherAPI.com documentation
- Submit issues to the project repository

## Roadmap

Completed features:
- ✅ Weather forecasts (1-14 days with hourly data)
- ✅ HTTP server mode for serving weather data
- ✅ OpenAPI 3.0 specification and documentation
- ✅ Command line forecast options (-f<n>)
- ✅ Environment variable support for API keys

Future enhancements may include:
- Historical weather data
- Weather alerts and warnings integration
- Multiple location support in single request
- Configuration file support
- Weather data caching and persistence
- Database integration
- Authentication and rate limiting for server mode
- WebSocket support for real-time updates
- Batch weather requests
- Weather map integration