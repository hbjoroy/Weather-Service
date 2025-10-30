# Docker Build Guide

This guide covers building Docker images for the Weather Service stack with multi-platform support.

## Multi-Platform Support

The Docker images are built to support multiple CPU architectures:

- **linux/amd64** - Intel/AMD x86_64 processors
- **linux/arm64** - ARM 64-bit processors (AWS Graviton, Apple Silicon, Raspberry Pi 4+)

This allows deployment on diverse infrastructure without rebuilding images.

## Quick Start

### Option 1: Automated Build Scripts

**Build and push to registry (multi-platform):**
```bash
export DOCKER_REGISTRY=your-dockerhub-username
export VERSION=1.0.0
./build-images.sh
```

**Build locally (current platform only):**
```bash
./build-local.sh
```

### Option 2: Manual Build

**Single platform (fastest for local testing):**
```bash
cd weather
docker build -t hbjoroy/weather-service:1.0.0 .

cd ../weather-dashboard
# Build frontend first
cd frontend && npm install && npm run build && cd ..
docker build -t hbjoroy/weather-dashboard:1.0.0 .
```

**Multi-platform (for production):**
```bash
# One-time setup: Create buildx builder
docker buildx create --name weather-builder --use
docker buildx inspect --bootstrap

# Build weather service for multiple platforms
cd weather
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t hbjoroy/weather-service:1.0.0 \
  -t hbjoroy/weather-service:latest \
  --push \
  .

# Build dashboard
cd ../weather-dashboard/frontend
npm install && npm run build
cd ..
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t hbjoroy/weather-dashboard:1.0.0 \
  -t hbjoroy/weather-dashboard:latest \
  --push \
  .
```

## Image Details

### Weather Service
- **Base Image**: gcc:12 (builder), debian:12-slim (runtime)
- **Size**: ~90MB (runtime)
- **Port**: 8080
- **User**: Non-root (UID 1000)
- **Dependencies**: libcurl4, libcjson1, libmicrohttpd12

### Weather Dashboard
- **Base Image**: gcc:12 (builder), debian:12-slim (runtime)
- **Size**: ~100MB (runtime + static files)
- **Port**: 3001
- **User**: Non-root (UID 1000)
- **Dependencies**: libcurl4, libcjson1, libmicrohttpd12

## Build Architecture

Both services use **multi-stage builds**:

1. **Builder Stage** (gcc:12)
   - Compiles C source code
   - Includes all build tools and development libraries
   - Discarded after compilation

2. **Runtime Stage** (debian:12-slim)
   - Minimal base image
   - Only runtime libraries
   - Non-root user
   - Final compiled binary

This approach minimizes image size while maintaining security.

## Build Arguments

The Dockerfiles support platform-aware building:

- `BUILDPLATFORM` - Platform performing the build
- `TARGETPLATFORM` - Platform the image will run on
- `TARGETARCH` - Target architecture (amd64, arm64)

Example:
```bash
docker buildx build \
  --platform linux/arm64 \
  --build-arg TARGETARCH=arm64 \
  -t myimage:latest .
```

## Testing Images Locally

### Run Weather Service
```bash
docker run -d \
  --name weather-service \
  -p 8080:8080 \
  -e WEATHERAPI_KEY=your_key \
  -e SLACK_BOT_TOKEN=your_token \
  -e SLACK_APP_ID=your_app_id \
  hbjoroy/weather-service:1.0.0

# Test
curl http://localhost:8080/health
```

### Run Weather Dashboard
```bash
docker run -d \
  --name weather-dashboard \
  -p 3001:3001 \
  -e WEATHER_SERVICE_URL=http://weather-service:8080 \
  hbjoroy/weather-dashboard:1.0.0

# Access at http://localhost:3001
```

### Run with Docker Compose
```bash
# See docker-compose.yml in repository root
docker-compose up -d
```

## Verifying Multi-Platform Images

After pushing multi-platform images:

```bash
# Inspect the manifest
docker buildx imagetools inspect hbjoroy/weather-service:1.0.0

# Expected output:
# Name:      docker.io/hbjoroy/weather-service:1.0.0
# MediaType: application/vnd.docker.distribution.manifest.list.v2+json
# Digest:    sha256:...
# 
# Manifests:
#   Name:      docker.io/hbjoroy/weather-service:1.0.0@sha256:...
#   MediaType: application/vnd.docker.distribution.manifest.v2+json
#   Platform:  linux/amd64
#   
#   Name:      docker.io/hbjoroy/weather-service:1.0.0@sha256:...
#   MediaType: application/vnd.docker.distribution.manifest.v2+json
#   Platform:  linux/arm64
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build Multi-Platform Images

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up QEMU
        uses: docker/setup-qemu-action@v2
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v2
      
      - name: Login to Docker Hub
        uses: docker/login-action@v2
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}
      
      - name: Build and push weather-service
        uses: docker/build-push-action@v4
        with:
          context: ./weather
          platforms: linux/amd64,linux/arm64
          push: true
          tags: |
            hbjoroy/weather-service:${{ github.ref_name }}
            hbjoroy/weather-service:latest
      
      - name: Build frontend
        run: |
          cd weather-dashboard/frontend
          npm install
          npm run build
      
      - name: Build and push weather-dashboard
        uses: docker/build-push-action@v4
        with:
          context: ./weather-dashboard
          platforms: linux/amd64,linux/arm64
          push: true
          tags: |
            hbjoroy/weather-dashboard:${{ github.ref_name }}
            hbjoroy/weather-dashboard:latest
```

## Troubleshooting

### QEMU for Cross-Platform Builds

If building on amd64 for arm64 (or vice versa), install QEMU:

```bash
# Install QEMU user-static
docker run --privileged --rm tonistiigi/binfmt --install all

# Verify
docker buildx ls
```

### Build Cache

Speed up builds with cache:

```bash
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --cache-from type=registry,ref=hbjoroy/weather-service:buildcache \
  --cache-to type=registry,ref=hbjoroy/weather-service:buildcache,mode=max \
  -t hbjoroy/weather-service:1.0.0 \
  --push \
  .
```

### Platform-Specific Issues

If a build fails for one platform:

```bash
# Build platforms separately
docker buildx build --platform linux/amd64 -t myimage:amd64 .
docker buildx build --platform linux/arm64 -t myimage:arm64 .

# Create manifest manually
docker manifest create myimage:latest myimage:amd64 myimage:arm64
docker manifest push myimage:latest
```

## Security Best Practices

1. **Non-root user**: Both images run as UID 1000
2. **Minimal base**: debian:12-slim reduces attack surface
3. **No secrets in image**: Use environment variables or Kubernetes secrets
4. **Regular updates**: Rebuild monthly to get security patches
5. **Scan images**: Use `docker scan` or Trivy

```bash
# Scan for vulnerabilities
docker scan hbjoroy/weather-service:1.0.0

# Or with Trivy
trivy image hbjoroy/weather-service:1.0.0
```

## Size Optimization

Current sizes:
- **weather-service**: ~90MB
- **weather-dashboard**: ~100MB

Already optimized via:
- Multi-stage builds
- Minimal base image (debian:12-slim vs full debian)
- Only runtime dependencies
- No build tools in final image

Further optimization possible with Alpine Linux, but may require additional work for glibc compatibility.

## Registry Options

Supports any Docker-compatible registry:

- **Docker Hub**: `docker.io/username/image`
- **GitHub Container Registry**: `ghcr.io/username/image`
- **AWS ECR**: `aws_account_id.dkr.ecr.region.amazonaws.com/image`
- **Google GCR**: `gcr.io/project-id/image`
- **Azure ACR**: `myregistry.azurecr.io/image`

Update `DOCKER_REGISTRY` environment variable accordingly.
