#!/bin/bash

# Weather Dashboard Setup and Test Script

set -e  # Exit on any error

echo "🌤️  Weather Dashboard Setup"
echo "=========================="

# Check if we're in the right directory
if [ ! -f "README.md" ]; then
    echo "❌ Please run this script from the weather-dashboard directory"
    exit 1
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}📦 Building backend...${NC}"
cd backend

# Check dependencies
echo "Checking dependencies..."
if ! pkg-config --exists libcurl; then
    echo -e "${YELLOW}⚠️  libcurl not found. Installing dependencies...${NC}"
    make deps
fi

if ! pkg-config --exists libcjson; then
    echo -e "${YELLOW}⚠️  libcjson not found. Installing dependencies...${NC}"
    make deps
fi

if ! pkg-config --exists libmicrohttpd; then
    echo -e "${YELLOW}⚠️  libmicrohttpd not found. Installing dependencies...${NC}"
    make deps
fi

# Build backend
echo "Building C backend..."
make clean
make

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Backend build successful${NC}"
else
    echo -e "${RED}❌ Backend build failed${NC}"
    exit 1
fi

cd ..

echo -e "${BLUE}🔧 Setting up frontend...${NC}"
cd frontend

# Check if Node.js is available
if ! command -v npm &> /dev/null; then
    echo -e "${YELLOW}⚠️  Node.js/npm not found. Please install Node.js first.${NC}"
    echo "Visit: https://nodejs.org/"
    echo -e "${BLUE}ℹ️  The backend can still run with the static HTML demo${NC}"
else
    echo "Installing frontend dependencies..."
    npm install
    
    echo "Building frontend..."
    npm run build
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Frontend build successful${NC}"
        
        # Copy built frontend to backend static directory
        echo "Copying frontend to backend static directory..."
        cp -r dist/* ../backend/static/ 2>/dev/null || echo "Using demo HTML instead"
    else
        echo -e "${YELLOW}⚠️  Frontend build failed, using demo HTML${NC}"
    fi
fi

cd ..

echo -e "${BLUE}🚀 Starting services...${NC}"

# Check if weather service is running
if ! curl -s http://localhost:8080/health > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠️  Weather service not running on port 8080${NC}"
    echo "Please start the weather service first:"
    echo "  cd ../weather"
    echo "  export WEATHERAPI_KEY=your_api_key"
    echo "  ./build/weather_service -s -p 8080"
    echo ""
    echo -e "${BLUE}ℹ️  Continuing with dashboard server (some features may not work)${NC}"
else
    echo -e "${GREEN}✅ Weather service is running${NC}"
fi

# Start dashboard server
echo "Starting Weather Dashboard server..."
cd backend

# Start server in background for testing
./build/weather-dashboard-server -c -v &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Test if server is running
if curl -s http://localhost:3001/api/profile > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Dashboard server started successfully${NC}"
    
    echo ""
    echo -e "${GREEN}🎉 Setup complete!${NC}"
    echo ""
    echo -e "${BLUE}📝 Access the application:${NC}"
    echo "  🌐 Web Interface: http://localhost:3001"
    echo "  📊 API Endpoints:"
    echo "    • GET  /api/profile"
    echo "    • PUT  /api/profile"
    echo "    • GET  /api/weather/current"
    echo "    • GET  /api/weather/forecast"
    echo ""
    echo -e "${BLUE}👤 Default User Profile:${NC}"
    echo "  📛 Name: Χαράλαμπους Μπιγγ (UTF-8 Greek)"
    echo "  🌡️  Temperature Unit: Celsius"
    echo "  📍 Default Location: Athens"
    echo ""
    echo -e "${BLUE}🧪 Testing API endpoints...${NC}"
    
    # Test profile endpoint
    echo "Testing profile endpoint..."
    PROFILE_RESPONSE=$(curl -s http://localhost:3001/api/profile)
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Profile API working${NC}"
        echo "$PROFILE_RESPONSE" | jq . 2>/dev/null || echo "$PROFILE_RESPONSE"
    else
        echo -e "${RED}❌ Profile API failed${NC}"
    fi
    
    echo ""
    echo -e "${YELLOW}💡 To stop the server: kill $SERVER_PID${NC}"
    echo -e "${YELLOW}💡 Or press Ctrl+C to stop this script and the server${NC}"
    
    # Keep script running
    echo ""
    echo "Server is running. Press Ctrl+C to stop..."
    wait $SERVER_PID
    
else
    echo -e "${RED}❌ Dashboard server failed to start${NC}"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi