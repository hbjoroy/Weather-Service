# Weather Dashboard

A full-stack weather application with C backend and TypeScript/Vue.js frontend that consumes the weather service API.

## Architecture

- **Backend**: C HTTP server using libmicrohttpd
- **Frontend**: TypeScript + Vue.js 3 + Vite
- **Communication**: REST API calls to weather service
- **Encoding**: Full UTF-8 support for international users

## Features

- User profiles with temperature unit preferences (Celsius/Fahrenheit)
- Default user: "Χαράλαμπους Μπιγγ" (Greek name with UTF-8 characters)
- Current weather tab
- Weather forecast tab with configurable parameters
- Global location input
- Responsive web interface

## Project Structure

```
weather-dashboard/
├── backend/                 # C HTTP server
│   ├── src/                # Source files
│   ├── include/            # Header files
│   ├── static/             # Static web files (served by C backend)
│   ├── build/              # Build artifacts
│   └── Makefile           # Build configuration
├── frontend/               # Vue.js TypeScript frontend
│   ├── src/               # TypeScript source files
│   ├── public/            # Public assets
│   ├── dist/              # Built frontend (copied to backend/static)
│   └── package.json       # NPM dependencies
└── README.md              # This file
```

## Development Workflow

1. **Backend Development**: C server serves static files and provides API proxy
2. **Frontend Development**: Vue.js with Vite for hot reloading
3. **Production Build**: Frontend builds to `dist/`, copied to `backend/static/`

## Getting Started

### Prerequisites

- C development tools (gcc, make)
- libmicrohttpd-dev
- libcjson-dev
- libcurl-dev
- Node.js and npm
- Weather service running on localhost:8080

### Backend Setup

```bash
cd backend
make
./build/weather-dashboard-server
```

### Frontend Setup

```bash
cd frontend
npm install
npm run dev
```

### Production Build

```bash
# Build frontend
cd frontend
npm run build

# Copy to backend static files
cp -r dist/* ../backend/static/

# Start production server
cd ../backend
./build/weather-dashboard-server
```

## API Endpoints

The backend server provides:

- Static file serving for the Vue.js application
- Proxy endpoints to the weather service
- User profile management (in-memory for now)
- CORS support for development

## Default User Profile

- **Name**: Χαράλαμπους Μπιγγ (UTF-8 Greek characters)
- **Temperature Unit**: Celsius
- **Default Location**: Athens (can be changed in UI)

## Development

The application is designed for easy development:

1. Start the weather service: `./weather/build/weather_service -s -p 8080`
2. Start the dashboard backend: `./backend/build/weather-dashboard-server`
3. Start the frontend dev server: `cd frontend && npm run dev`
4. Access the application at `http://localhost:3000`

## License

Open source - feel free to use and modify.