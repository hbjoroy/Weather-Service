#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "weather_api.h"
#include "http_server.h"

#define DEFAULT_BASE_URL "https://api.weatherapi.com/v1"
#define DEFAULT_TIMEOUT 30
#define DEFAULT_SERVER_PORT 8080
#define DEFAULT_MAX_CONNECTIONS 100

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <location>\n", program_name);
    printf("\n");
    printf("Fetch current weather data or forecast for a location.\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -k, --key <API_KEY>     WeatherAPI.com API key\n");
    printf("  -f, --forecast <DAYS>   Get forecast for N days (1-14, default: current weather)\n");
    printf("  -H, --hourly            Show hourly forecast details (only with -f)\n");
    printf("  -a, --aqi               Include air quality data\n");
    printf("  -A, --alerts            Include weather alerts (only with -f)\n");
    printf("  -s, --server            Run as HTTP web service\n");
    printf("  -p, --port <PORT>       Server port (default: %d, only with -s)\n", DEFAULT_SERVER_PORT);
    printf("  -b, --bind <ADDRESS>    Bind address (default: 0.0.0.0, only with -s)\n");
    printf("  -v, --verbose           Enable verbose logging (only with -s)\n");
    printf("  -C, --cors              Enable CORS headers (only with -s)\n");
    printf("  -S, --slack <TOKEN>     Slack Bot OAuth Token (only with -s)\n");
    printf("  -I, --app-id <ID>       Slack App ID to ignore own messages (only with -s)\n");
    printf("  -u, --url <URL>         Base API URL (default: %s)\n", DEFAULT_BASE_URL);
    printf("  -t, --timeout <SEC>     Request timeout in seconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -h, --help              Show this help message\n");
    printf("\n");
    printf("API KEY:\n");
    printf("  The API key can be provided in two ways:\n");
    printf("  1. Command line option: -k YOUR_API_KEY\n");
    printf("  2. Environment variable: export WEATHERAPI_KEY=YOUR_API_KEY\n");
    printf("  Command line option takes precedence over environment variable.\n");
    printf("\n");
    printf("SLACK BOT TOKEN:\n");
    printf("  For Slack integration, provide the bot token:\n");
    printf("  1. Command line option: -S xoxb-your-token\n");
    printf("  2. Environment variable: export SLACK_BOT_TOKEN=xoxb-your-token\n");
    printf("\n");
    printf("SLACK APP ID:\n");
    printf("  To prevent duplicate messages, provide your Slack App ID:\n");
    printf("  1. Command line option: -I A01234567\n");
    printf("  2. Environment variable: export SLACK_APP_ID=A01234567\n");
    printf("  Find it at: https://api.slack.com/apps -> Your App -> Basic Information\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("  # Current weather:\n");
    printf("  %s \"London\"\n", program_name);
    printf("  %s -k YOUR_API_KEY \"London\"\n", program_name);
    printf("\n");
    printf("  # Weather forecast:\n");
    printf("  %s -f5 \"London\"              # 5-day forecast\n", program_name);
    printf("  %s -f3 -H \"New York\"         # 3-day forecast with hourly details\n", program_name);
    printf("  %s -f7 -a -A \"Tokyo\"         # 7-day forecast with AQI and alerts\n", program_name);
    printf("\n");
    printf("  # Web service mode:\n");
    printf("  %s -s                         # Start server on port 8080\n", program_name);
    printf("  %s -s -p 3000 -v              # Start server on port 3000 with verbose logging\n", program_name);
    printf("  %s -s -b 127.0.0.1 -C         # Start server bound to localhost with CORS\n", program_name);
    printf("\n");
    printf("  # With Slack integration:\n");
    printf("  export SLACK_BOT_TOKEN=xoxb-your-token-here\n");
    printf("  export SLACK_APP_ID=A01234567\n");
    printf("  %s -s -v                      # Start server with Slack integration\n", program_name);
    printf("\n");
    printf("  # Using environment variable:\n");
    printf("  export WEATHERAPI_KEY=YOUR_API_KEY\n");
    printf("  %s \"London\"\n", program_name);
    printf("  %s -f5 \"New York\"\n", program_name);
    printf("\n");
    printf("  # Other examples:\n");
    printf("  %s \"37.7749,-122.4194\"\n", program_name);
    printf("  %s \"Paros\"\n", program_name);
    printf("\n");
    printf("LOCATION FORMATS:\n");
    printf("  - City name: \"London\", \"New York\"\n");
    printf("  - City, State: \"London, UK\", \"New York, NY\"\n");
    printf("  - Coordinates: \"37.7749,-122.4194\"\n");
    printf("  - IP address: \"auto:ip\" (for current location)\n");
    printf("\n");
    printf("Get your free API key at: https://www.weatherapi.com/\n");
}

static void print_error_help(void) {
    printf("\nFor help, run with --help option.\n");
    printf("Make sure you have a valid API key from https://www.weatherapi.com/\n");
}

int main(int argc, char *argv[]) {
    char *api_key = NULL;
    char *location = NULL;
    char *base_url = DEFAULT_BASE_URL;
    char *bind_address = "0.0.0.0";
    char *slack_bot_token = NULL;
    char *slack_app_id = NULL;
    int include_aqi = 0;
    int include_alerts = 0;
    int show_hourly = 0;
    int forecast_days = 0;  // 0 = current weather, >0 = forecast
    int timeout = DEFAULT_TIMEOUT;
    int server_mode = 0;
    int server_port = DEFAULT_SERVER_PORT;
    int verbose = 0;
    int enable_cors = 0;
    
    // Parse command line options
    static struct option long_options[] = {
        {"key",      required_argument, 0, 'k'},
        {"forecast", required_argument, 0, 'f'},
        {"hourly",   no_argument,       0, 'H'},
        {"aqi",      no_argument,       0, 'a'},
        {"alerts",   no_argument,       0, 'A'},
        {"server",   no_argument,       0, 's'},
        {"port",     required_argument, 0, 'p'},
        {"bind",     required_argument, 0, 'b'},
        {"verbose",  no_argument,       0, 'v'},
        {"cors",     no_argument,       0, 'C'},
        {"slack",    required_argument, 0, 'S'},
        {"app-id",   required_argument, 0, 'I'},
        {"url",      required_argument, 0, 'u'},
        {"timeout",  required_argument, 0, 't'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "k:f:HaAsp:b:vCS:I:u:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'k':
                api_key = optarg;
                break;
            case 'f':
                forecast_days = atoi(optarg);
                if (forecast_days < 1 || forecast_days > 14) {
                    fprintf(stderr, "Error: Forecast days must be between 1 and 14. Got: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'H':
                show_hourly = 1;
                break;
            case 'a':
                include_aqi = 1;
                break;
            case 'A':
                include_alerts = 1;
                break;
            case 's':
                server_mode = 1;
                break;
            case 'p':
                server_port = atoi(optarg);
                if (server_port <= 0 || server_port > 65535) {
                    fprintf(stderr, "Error: Invalid port number: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'b':
                bind_address = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'C':
                enable_cors = 1;
                break;
            case 'S':
                slack_bot_token = optarg;
                break;
            case 'I':
                slack_app_id = optarg;
                break;
                enable_cors = 1;
                break;
            case 'u':
                base_url = optarg;
                break;
            case 't':
                timeout = atoi(optarg);
                if (timeout <= 0) {
                    fprintf(stderr, "Error: Invalid timeout value: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case '?':
                print_error_help();
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Error: Unknown option\n");
                print_error_help();
                return EXIT_FAILURE;
        }
    }
    
    // Check for API key - try command line first, then environment variable
    if (!api_key) {
        api_key = getenv("WEATHERAPI_KEY");
        if (api_key) {
            printf("Using API key from WEATHERAPI_KEY environment variable\n");
        }
    }
    
    if (!api_key) {
        fprintf(stderr, "Error: API key is required.\n");
        fprintf(stderr, "Provide it using:\n");
        fprintf(stderr, "  - Command line: -k or --key option\n");
        fprintf(stderr, "  - Environment variable: export WEATHERAPI_KEY=your_key\n");
        print_error_help();
        return EXIT_FAILURE;
    }
    
    // Check for Slack bot token (optional, only used in server mode)
    if (!slack_bot_token) {
        slack_bot_token = getenv("SLACK_BOT_TOKEN");
        if (slack_bot_token && verbose) {
            printf("Using Slack bot token from SLACK_BOT_TOKEN environment variable\n");
        }
    }
    
    // Check for Slack app ID (optional, only used in server mode)
    if (!slack_app_id) {
        slack_app_id = getenv("SLACK_APP_ID");
        if (slack_app_id && verbose) {
            printf("Using Slack app ID from SLACK_APP_ID environment variable\n");
        }
    }
    
    // Server mode validation
    if (server_mode) {
        // In server mode, location is not required
        if (forecast_days > 0) {
            fprintf(stderr, "Error: Forecast options (-f, -H, -A) are not used in server mode.\n");
            fprintf(stderr, "Use JSON requests to the /forecast endpoint instead.\n");
            print_error_help();
            return EXIT_FAILURE;
        }
        
        printf("Weather API Server Starting...\n");
        printf("Port: %d\n", server_port);
        printf("Bind Address: %s\n", bind_address);
        printf("Verbose Logging: %s\n", verbose ? "Yes" : "No");
        printf("CORS Enabled: %s\n", enable_cors ? "Yes" : "No");
        printf("Slack Integration: %s\n", slack_bot_token ? "Enabled" : "Disabled");
        if (slack_app_id) {
            printf("Slack App ID: %s (will ignore own messages)\n", slack_app_id);
        }
        printf("API Base URL: %s\n", base_url);
        printf("Timeout: %d seconds\n\n", timeout);
        
        // Configure weather API
        weather_config_t weather_config;
        strncpy(weather_config.api_key, api_key, sizeof(weather_config.api_key) - 1);
        weather_config.api_key[sizeof(weather_config.api_key) - 1] = '\0';
        strncpy(weather_config.base_url, base_url, sizeof(weather_config.base_url) - 1);
        weather_config.base_url[sizeof(weather_config.base_url) - 1] = '\0';
        weather_config.timeout = timeout;
        
        // Configure server
        server_config_t server_config;
        server_config.port = server_port;
        server_config.max_connections = DEFAULT_MAX_CONNECTIONS;
        strncpy(server_config.bind_address, bind_address, sizeof(server_config.bind_address) - 1);
        server_config.bind_address[sizeof(server_config.bind_address) - 1] = '\0';
        server_config.enable_cors = enable_cors;
        
        // Set Slack bot token if provided
        if (slack_bot_token) {
            strncpy(server_config.slack_bot_token, slack_bot_token, sizeof(server_config.slack_bot_token) - 1);
            server_config.slack_bot_token[sizeof(server_config.slack_bot_token) - 1] = '\0';
        } else {
            server_config.slack_bot_token[0] = '\0';
        }
        
        // Set Slack app ID if provided
        if (slack_app_id) {
            strncpy(server_config.slack_app_id, slack_app_id, sizeof(server_config.slack_app_id) - 1);
            server_config.slack_app_id[sizeof(server_config.slack_app_id) - 1] = '\0';
        } else {
            server_config.slack_app_id[0] = '\0';
        }
        
        // Initialize and start server
        if (http_server_init(&server_config, &weather_config) != 0) {
            fprintf(stderr, "Error: Failed to initialize HTTP server\n");
            return EXIT_FAILURE;
        }
        
        http_server_set_verbose(verbose);
        
        if (http_server_start() != 0) {
            fprintf(stderr, "Error: Failed to start HTTP server\n");
            http_server_cleanup();
            return EXIT_FAILURE;
        }
        
        http_server_cleanup();
        printf("Server stopped.\n");
        return EXIT_SUCCESS;
    }
    
    // CLI mode validation
    if (optind >= argc) {
        fprintf(stderr, "Error: Location is required in CLI mode.\n");
        print_error_help();
        return EXIT_FAILURE;
    }
    
    location = argv[optind];
    
    // Validate location is not empty
    if (strlen(location) == 0) {
        fprintf(stderr, "Error: Location cannot be empty.\n");
        print_error_help();
        return EXIT_FAILURE;
    }
    
    // Validate hourly option
    if (show_hourly && forecast_days == 0) {
        fprintf(stderr, "Error: --hourly (-H) option requires forecast mode. Use -f<days> option.\n");
        print_error_help();
        return EXIT_FAILURE;
    }
    
    // Validate alerts option
    if (include_alerts && forecast_days == 0) {
        fprintf(stderr, "Error: --alerts (-A) option requires forecast mode. Use -f<days> option.\n");
        print_error_help();
        return EXIT_FAILURE;
    }
    
    printf("Weather Service Starting...\n");
    printf("Location: %s\n", location);
    if (forecast_days > 0) {
        printf("Mode: %d-day forecast\n", forecast_days);
        printf("Show Hourly: %s\n", show_hourly ? "Yes" : "No");
        printf("Include Alerts: %s\n", include_alerts ? "Yes" : "No");
    } else {
        printf("Mode: Current weather\n");
    }
    printf("Include AQI: %s\n", include_aqi ? "Yes" : "No");
    printf("API Base URL: %s\n", base_url);
    printf("Timeout: %d seconds\n\n", timeout);
    
    // Configure weather API
    weather_config_t config;
    strncpy(config.api_key, api_key, sizeof(config.api_key) - 1);
    config.api_key[sizeof(config.api_key) - 1] = '\0';
    strncpy(config.base_url, base_url, sizeof(config.base_url) - 1);
    config.base_url[sizeof(config.base_url) - 1] = '\0';
    config.timeout = timeout;
    
    // Initialize weather API
    if (weather_api_init(&config) != 0) {
        fprintf(stderr, "Error: Failed to initialize weather API\n");
        return EXIT_FAILURE;
    }
    
    // Fetch weather data
    if (forecast_days > 0) {
        // Fetch forecast data
        forecast_response_t forecast_response;
        printf("Fetching %d-day forecast...\n\n", forecast_days);
        
        if (weather_api_get_forecast(location, forecast_days, include_aqi, include_alerts, &forecast_response) != 0) {
            fprintf(stderr, "Error: Failed to fetch forecast data\n");
            fprintf(stderr, "Please check:\n");
            fprintf(stderr, "  - Your API key is valid\n");
            fprintf(stderr, "  - The location exists and is spelled correctly\n");
            fprintf(stderr, "  - Your internet connection is working\n");
            fprintf(stderr, "  - The WeatherAPI service is available\n");
            weather_api_cleanup();
            return EXIT_FAILURE;
        }
        
        // Display forecast information
        weather_print_forecast(&forecast_response, show_hourly);
        
        // Cleanup
        forecast_response_free(&forecast_response);
    } else {
        // Fetch current weather data
        weather_response_t response;
        printf("Fetching current weather data...\n\n");
        
        if (weather_api_get_current(location, include_aqi, &response) != 0) {
            fprintf(stderr, "Error: Failed to fetch weather data\n");
            fprintf(stderr, "Please check:\n");
            fprintf(stderr, "  - Your API key is valid\n");
            fprintf(stderr, "  - The location exists and is spelled correctly\n");
            fprintf(stderr, "  - Your internet connection is working\n");
            fprintf(stderr, "  - The WeatherAPI service is available\n");
            weather_api_cleanup();
            return EXIT_FAILURE;
        }
        
        // Display weather information
        weather_print_current(&response);
        
        // Cleanup
        weather_response_free(&response);
    }
    weather_api_cleanup();
    
    printf("\nWeather data fetched successfully!\n");
    return EXIT_SUCCESS;
}