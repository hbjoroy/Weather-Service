# Weather Service & Dashboard

A comprehensive weather application suite consisting of two C-based applications: a robust weather service API and a full-stack weather dashboard with Vue.js frontend.

## Remark

This is an experiment I did to try 'Vibe coding' for the first time. All code has been written by Claude Sonnet 4. Total time for the initial commit was about 3 hours. Two of those hours was spent on the front end Typescript/Vue.js, and the most frustrating bit was to get it handle state between pages/components.

## 🌟 Overview

This repository contains two complementary applications:

1. **Weather Service** - A standalone C application that fetches weather data from WeatherAPI.com and can operate both as a CLI tool and HTTP API server
2. **Weather Dashboard** - A full-stack web application with C backend and Vue.js/TypeScript frontend that provides a rich user interface for weather data

## 📁 Project Structure

```
Weather-Service/
├── weather/                    # Standalone weather service
│   ├── src/                   # C source files
│   ├── include/               # Header files
│   ├── build/                 # Build artifacts
│   ├── openapi.yaml          # API documentation
│   ├── Makefile              # Build configuration
│   └── README.md             # Weather service docs
├── weather-dashboard/         # Full-stack web application
│   ├── backend/              # C HTTP server
│   │   ├── src/             # C source files
│   │   ├── include/         # Header files
│   │   ├── static/          # Static web files
│   │   └── Makefile         # Build configuration
│   ├── frontend/            # Vue.js TypeScript frontend
│   │   ├── src/            # Vue components and logic
│   │   ├── public/         # Public assets
│   │   └── package.json    # NPM dependencies
│   ├── setup.sh            # Automated setup script
│   └── README.md           # Dashboard docs
└── README.md               # This file
```

## 🚀 Quick Start

### Prerequisites

Both applications require these system dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install -y libcurl4-openssl-dev libcjson-dev libmicrohttpd-dev

# CentOS/RHEL/Fedora
sudo dnf install -y libcurl-devel libcjson-devel libmicrohttpd-devel

# macOS (with Homebrew)
brew install curl cjson libmicrohttpd
```

### Weather Service API Key

Both applications require a WeatherAPI.com API key:

1. Get a free API key at [WeatherAPI.com](https://www.weatherapi.com/)
2. Set the environment variable:
```bash
export WEATHERAPI_KEY=your_api_key_here
```

### Option 1: Weather Dashboard (Recommended for Web UI)

The weather dashboard provides a complete web interface:

```bash
cd weather-dashboard
./setup.sh
```

This will:
- Build the C backend server
- Install frontend dependencies (Node.js required)
- Build the Vue.js frontend
- Start both services

Access the dashboard at http://localhost:3001

### Option 2: Standalone Weather Service

For CLI usage or API-only deployment:

```bash
cd weather
make

# CLI usage
./build/weather_service "London"
./build/weather_service -f5 "New York"  # 5-day forecast

# Server mode
./build/weather_service -s -p 8080
```

## 🔧 Applications Overview

### Weather Service

**Location**: `weather/`

A robust C application that can operate in two modes:

#### CLI Mode
- Get current weather or forecasts for any location
- Support for city names, coordinates, and IP-based location
- Rich command-line options for customization
- JSON output for scripting

```bash
# Examples
./weather_service "London"
./weather_service -f3 -H "New York"  # 3-day forecast with hourly details
./weather_service "37.7749,-122.4194"  # Coordinates
```

#### Server Mode
- RESTful HTTP API with OpenAPI 3.0 documentation
- Serves weather data to other applications
- Built with libmicrohttpd for high performance
- CORS support for web applications

**API Endpoints**:
- `GET /health` - Health check
- `GET /current?location=London` - Current weather
- `POST /current` - Current weather (JSON body)
- `GET /forecast?location=London&days=3` - Weather forecast
- `POST /forecast` - Weather forecast (JSON body)

### Weather Dashboard

**Location**: `weather-dashboard/`

A full-stack web application with rich user interface:

#### Backend (C)
- HTTP server built with libmicrohttpd
- Serves static files (Vue.js frontend)
- Proxies requests to weather service API
- UTF-8 support for international users

#### Frontend (Vue.js + TypeScript)
- Modern responsive web interface
- User profiles with temperature unit preferences (°C/°F)
- Current weather and forecast tabs
- Interactive weather charts
- Global location input
- Default demo user: "Χαράλαμπους Μπιγγ" (showcases UTF-8 support)

**Features**:
- 📱 Responsive design for mobile and desktop
- 🌡️ Temperature unit switching (Celsius/Fahrenheit)
- 📊 Interactive forecast charts
- 🌍 Global location search
- 👤 User profile management
- 🎨 Modern UI with Vue.js 3

## 🛠️ Development

### Building Weather Service

```bash
cd weather
make                    # Build the application
make test              # Test with London weather
make test-forecast     # Test 3-day forecast
make test-server       # Start in server mode
make clean             # Clean build artifacts
```

### Building Weather Dashboard

```bash
cd weather-dashboard

# Backend only
cd backend
make

# Full stack (recommended)
cd ..
./setup.sh
```

### Development Workflow

1. **Weather Service Development**: Direct C development with immediate CLI testing
2. **Dashboard Development**: 
   - Backend: C server development
   - Frontend: Vue.js with Vite hot reloading (`npm run dev`)
   - Production: Automated build and deployment

## 📋 API Documentation

The weather service includes comprehensive OpenAPI 3.0 documentation:
- View: `weather/openapi.yaml`
- When running server: http://localhost:8080 (serves the spec)

## 🔗 Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Vue.js Web    │    │  Dashboard C     │    │  Weather Service│
│   Frontend      │◄──►│  Backend         │◄──►│  API            │
│                 │    │  (Port 3001)     │    │  (Port 8080)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                                         │
                                                         ▼
                                                ┌─────────────────┐
                                                │  WeatherAPI.com │
                                                │  External API   │
                                                └─────────────────┘
```

## 🌐 Deployment

### Production Deployment

1. **Weather Service Only**:
```bash
cd weather
make
./build/weather_service -s -p 8080 -b 0.0.0.0
```

2. **Full Dashboard**:
```bash
cd weather-dashboard
./setup.sh
# Configure reverse proxy to point to localhost:3001
```

### Docker Support

Both applications can be containerized. Example Dockerfile patterns are included in respective directories.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly with both applications
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the individual application directories for details.

## 🔧 Troubleshooting

### Common Issues

1. **Missing API Key**:
   ```bash
   export WEATHERAPI_KEY=your_api_key_here
   ```

2. **Missing Dependencies**:
   ```bash
   # Use the provided dependency installation commands
   make deps  # In either weather/ or weather-dashboard/backend/
   ```

3. **Port Conflicts**:
   - Weather Service: Use `-p` flag to change from default 8080
   - Dashboard: Use `-p` flag to change from default 3001

4. **Build Failures**:
   - Ensure all dependencies are installed
   - Check that you have gcc and make installed
   - For frontend: Ensure Node.js and npm are installed

### Support

- Check individual README.md files in `weather/` and `weather-dashboard/` for specific documentation
- Review OpenAPI specification in `weather/openapi.yaml` for API details
- Use verbose mode (`-v`) for detailed logging in server mode

## 🎯 Use Cases

- **Weather Service**: Perfect for scripting, automation, or as a backend API for other applications
- **Weather Dashboard**: Ideal for end-user weather applications, demos, or as a starting point for weather-related web applications
- **Combined**: Use both for a complete weather solution with CLI tools and web interface