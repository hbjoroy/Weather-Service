#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include "http_server.h"
#include "oidc_client.h"

static volatile int running = 1;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig;
    printf("\nReceived shutdown signal...\n");
    running = 0;
}

// Print usage information
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Weather Dashboard Server - C backend for Vue.js frontend\n\n");
    printf("OPTIONS:\n");
    printf("  -p, --port <PORT>        Server port (default: 3001)\n");
    printf("  -b, --bind <ADDRESS>     Bind address (default: 127.0.0.1)\n");
    printf("  -s, --static <PATH>      Static files path (default: ./static)\n");
    printf("  -w, --weather <URL>      Weather service URL (default: http://localhost:8080)\n");
    printf("  -c, --cors               Enable CORS headers\n");
    printf("  -v, --verbose            Enable verbose logging\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("  # Start with default settings\n");
    printf("  %s\n\n", program_name);
    printf("  # Start on custom port with CORS enabled\n");
    printf("  %s -p 8090 -c -v\n\n", program_name);
    printf("  # Start with custom static path\n");
    printf("  %s -s /var/www/weather-dashboard -p 80\n\n", program_name);
}

int main(int argc, char *argv[]) {
    // Default configuration
    server_config_t config = {
        .port = 3001,
        .cors_enabled = false,
        .verbose = false
    };
    
    strcpy(config.bind_address, "127.0.0.1");
    strcpy(config.static_path, "./static");
    
    // Check for WEATHER_SERVICE_URL environment variable
    const char *weather_url_env = getenv("WEATHER_SERVICE_URL");
    if (weather_url_env) {
        strncpy(config.weather_service_url, weather_url_env, sizeof(config.weather_service_url) - 1);
        config.weather_service_url[sizeof(config.weather_service_url) - 1] = '\0';
    } else {
        strcpy(config.weather_service_url, "http://localhost:8080");
    }
    
    // Check for DATABASE_URL environment variable
    const char *database_url_env = getenv("DATABASE_URL");
    if (database_url_env) {
        strncpy(config.database_url, database_url_env, sizeof(config.database_url) - 1);
        config.database_url[sizeof(config.database_url) - 1] = '\0';
    } else {
        // Try to build from individual components (for Kubernetes secrets)
        const char *db_host = getenv("DATABASE_HOST");
        const char *db_port = getenv("DATABASE_PORT");
        const char *db_name = getenv("DATABASE_NAME");
        const char *db_user = getenv("DATABASE_USER");
        const char *db_pass = getenv("DATABASE_PASSWORD");
        const char *db_ssl = getenv("DATABASE_SSLMODE");
        
        if (db_host && db_port && db_name && db_user && db_pass) {
            // Build connection string from components
            snprintf(config.database_url, sizeof(config.database_url),
                    "host=%s port=%s dbname=%s user=%s password=%s sslmode=%s",
                    db_host, db_port, db_name, db_user, db_pass, 
                    db_ssl ? db_ssl : "prefer");
        } else {
            // Fallback to localhost default
            strcpy(config.database_url, "host=localhost port=5432 dbname=bjosoft-weather user=weather password=weather123");
        }
    }
    
    // Command line options
    static struct option long_options[] = {
        {"port",     required_argument, 0, 'p'},
        {"bind",     required_argument, 0, 'b'},
        {"static",   required_argument, 0, 's'},
        {"weather",  required_argument, 0, 'w'},
        {"cors",     no_argument,       0, 'c'},
        {"verbose",  no_argument,       0, 'v'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:b:s:w:cvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                config.port = atoi(optarg);
                if (config.port <= 0 || config.port > 65535) {
                    fprintf(stderr, "Error: Invalid port number %s\n", optarg);
                    return 1;
                }
                break;
            case 'b':
                strncpy(config.bind_address, optarg, sizeof(config.bind_address) - 1);
                config.bind_address[sizeof(config.bind_address) - 1] = '\0';
                break;
            case 's':
                strncpy(config.static_path, optarg, sizeof(config.static_path) - 1);
                config.static_path[sizeof(config.static_path) - 1] = '\0';
                break;
            case 'w':
                strncpy(config.weather_service_url, optarg, sizeof(config.weather_service_url) - 1);
                config.weather_service_url[sizeof(config.weather_service_url) - 1] = '\0';
                break;
            case 'c':
                config.cors_enabled = true;
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            default:
                abort();
        }
    }
    
    // Print startup information
    printf("Weather Dashboard Server v1.0\n");
    printf("==============================\n");
    printf("Configuration:\n");
    printf("  Port: %d\n", config.port);
    printf("  Bind Address: %s\n", config.bind_address);
    printf("  Static Path: %s\n", config.static_path);
    printf("  Weather Service: %s\n", config.weather_service_url);
    printf("  CORS Enabled: %s\n", config.cors_enabled ? "Yes" : "No");
    printf("  Verbose Logging: %s\n", config.verbose ? "Yes" : "No");
    printf("  Default User: Χαράλαμπους Μπιγγ (UTF-8 support enabled)\n");
    printf("\n");
    
    // Initialize OIDC if configured
    const char *oidc_issuer = getenv("OIDC_ISSUER");
    const char *oidc_client_id = getenv("OIDC_CLIENT_ID");
    const char *oidc_client_secret = getenv("OIDC_CLIENT_SECRET");
    const char *oidc_redirect_uri = getenv("OIDC_REDIRECT_URI");
    
    if (oidc_issuer && oidc_client_id && oidc_client_secret && oidc_redirect_uri) {
        printf("Initializing OIDC authentication...\n");
        if (oidc_init(oidc_issuer, oidc_client_id, oidc_client_secret, oidc_redirect_uri)) {
            printf("OIDC authentication enabled\n\n");
        } else {
            fprintf(stderr, "Warning: Failed to initialize OIDC\n\n");
        }
    } else {
        printf("OIDC not configured (using fake login for development)\n\n");
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Start the server
    if (http_server_start(&config) != 0) {
        fprintf(stderr, "Failed to start server\n");
        oidc_cleanup();
        return 1;
    }
    
    printf("\nServer is running. Access the dashboard at:\n");
    printf("  http://%s:%d/\n", 
           strcmp(config.bind_address, "0.0.0.0") == 0 ? "localhost" : config.bind_address,
           config.port);
    printf("\nAPI endpoints:\n");
    printf("  GET  /api/auth/login      - Initiate OIDC login\n");
    printf("  GET  /api/auth/callback   - OIDC callback handler\n");
    printf("  GET  /api/profile         - Get user profile\n");
    printf("  PUT  /api/profile         - Update user profile\n");
    printf("  POST /api/logout          - Logout\n");
    printf("  GET  /api/weather/current - Get current weather\n");
    printf("  GET  /api/weather/forecast - Get weather forecast\n");
    printf("\nPress Ctrl+C to stop the server\n\n");
    
    // Main loop
    while (running) {
        sleep(1);
    }
    
    // Cleanup
    http_server_stop();
    oidc_cleanup();
    
    return 0;
}