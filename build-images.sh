#!/bin/bash
set -e

# Multi-platform Docker image build script
# Builds images for both amd64 and arm64 architectures

# Configuration
REGISTRY="${DOCKER_REGISTRY:-hbjoroy}"
VERSION="${VERSION:-1.0.0}"
PLATFORMS="linux/amd64,linux/arm64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Multi-Platform Docker Build ===${NC}"
echo "Registry: $REGISTRY"
echo "Version: $VERSION"
echo "Platforms: $PLATFORMS"
echo ""

# Check if buildx is available
if ! sudo docker buildx version &> /dev/null; then
    echo -e "${RED}Error: docker buildx is not available${NC}"
    echo "Install it with: docker buildx install"
    exit 1
fi

# Create buildx builder if it doesn't exist
BUILDER_NAME="weather-builder"
if ! sudo docker buildx inspect $BUILDER_NAME &> /dev/null; then
    echo -e "${YELLOW}Creating buildx builder: $BUILDER_NAME${NC}"
    sudo docker buildx create --name $BUILDER_NAME --driver docker-container --use
    sudo docker buildx inspect --bootstrap
else
    echo -e "${GREEN}Using existing builder: $BUILDER_NAME${NC}"
    sudo docker buildx use $BUILDER_NAME
fi

# Function to build and push image
build_image() {
    local context=$1
    local image_name=$2
    local dockerfile=${3:-Dockerfile}
    
    echo ""
    echo -e "${GREEN}Building $image_name${NC}"
    echo "Context: $context"
    echo "Dockerfile: $context/$dockerfile"
    echo ""
    
    sudo docker buildx build \
        --platform $PLATFORMS \
        --tag $REGISTRY/$image_name:$VERSION \
        --tag $REGISTRY/$image_name:latest \
        --file $context/$dockerfile \
        --push \
        $context
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Successfully built and pushed $image_name${NC}"
    else
        echo -e "${RED}✗ Failed to build $image_name${NC}"
        exit 1
    fi
}

# Build Weather Service
build_image "weather" "weather-service"

# Build Weather Dashboard
# Note: Make sure frontend is built first!
if [ ! -d "weather-dashboard/backend/static/index.html" ]; then
    echo -e "${YELLOW}Warning: Frontend static files not found${NC}"
    echo "Building frontend first..."
    cd weather-dashboard/frontend
    npm install
    npm run build
    cd ../..
fi

build_image "weather-dashboard" "weather-dashboard"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo "Images pushed:"
echo "  $REGISTRY/weather-service:$VERSION"
echo "  $REGISTRY/weather-service:latest"
echo "  $REGISTRY/weather-dashboard:$VERSION"
echo "  $REGISTRY/weather-dashboard:latest"
echo ""
echo "Supported platforms: $PLATFORMS"
echo ""
echo "To inspect manifest:"
echo "  docker buildx imagetools inspect $REGISTRY/weather-service:$VERSION"
