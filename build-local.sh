#!/bin/bash
set -e

# Local multi-platform build (load to local Docker)
# This script builds for multiple platforms but only loads the current platform

# Configuration
REGISTRY="${DOCKER_REGISTRY:-hbjoroy}"
VERSION="${VERSION:-1.0.0}"

# Detect current platform
CURRENT_PLATFORM=$(docker version -f '{{.Server.Os}}/{{.Server.Arch}}')

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== Local Multi-Platform Build ===${NC}"
echo "Building for current platform: $CURRENT_PLATFORM"
echo "Registry: $REGISTRY"
echo "Version: $VERSION"
echo ""

# Check if buildx is available
if ! docker buildx version &> /dev/null; then
    echo -e "${YELLOW}buildx not available, using regular docker build${NC}"
    USE_BUILDX=false
else
    USE_BUILDX=true
fi

# Build Weather Service
echo -e "${GREEN}Building weather-service${NC}"
cd weather

if [ "$USE_BUILDX" = true ]; then
    docker buildx build \
        --platform $CURRENT_PLATFORM \
        --tag $REGISTRY/weather-service:$VERSION \
        --tag $REGISTRY/weather-service:latest \
        --load \
        .
else
    docker build \
        --tag $REGISTRY/weather-service:$VERSION \
        --tag $REGISTRY/weather-service:latest \
        .
fi

cd ..

# Build Weather Dashboard
echo -e "${GREEN}Building weather-dashboard${NC}"

# Check if frontend is built
if [ ! -f "weather-dashboard/backend/static/index.html" ]; then
    echo -e "${YELLOW}Building frontend first...${NC}"
    cd weather-dashboard/frontend
    npm install
    npm run build
    cd ../..
fi

cd weather-dashboard

if [ "$USE_BUILDX" = true ]; then
    docker buildx build \
        --platform $CURRENT_PLATFORM \
        --tag $REGISTRY/weather-dashboard:$VERSION \
        --tag $REGISTRY/weather-dashboard:latest \
        --load \
        .
else
    docker build \
        --tag $REGISTRY/weather-dashboard:$VERSION \
        --tag $REGISTRY/weather-dashboard:latest \
        .
fi

cd ..

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo "Images built for $CURRENT_PLATFORM:"
echo "  $REGISTRY/weather-service:$VERSION"
echo "  $REGISTRY/weather-dashboard:$VERSION"
