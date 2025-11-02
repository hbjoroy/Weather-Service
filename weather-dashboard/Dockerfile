# Multi-stage build for Weather Dashboard Backend
# Supports both amd64 and arm64 architectures via QEMU emulation
FROM gcc:12 AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    libcjson-dev \
    libmicrohttpd-dev \
    libpq-dev \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy backend source files
COPY backend/src/ src/
COPY backend/include/ include/
COPY backend/Makefile .

# Build the backend
RUN make clean && make

# Runtime stage
FROM debian:12-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libcurl4 \
    libcjson1 \
    libmicrohttpd12 \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -r -u 1000 -g root dashboard && \
    mkdir -p /app/static && \
    chown -R dashboard:root /app

WORKDIR /app

# Copy built binary
COPY --from=builder /build/build/weather-dashboard-server /app/weather-dashboard-server

# Copy static files (built frontend)
COPY backend/static/ /app/static/

# Switch to non-root user
USER dashboard

# Expose port
EXPOSE 3001

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["/app/weather-dashboard-server", "--help"] || exit 1

# Set default environment variables
ENV WEATHER_SERVICE_URL="http://weather-service:8080" \
    PORT=3001

# Run the dashboard server
CMD ["/app/weather-dashboard-server", "-p", "3001", "-b", "0.0.0.0", "-v", "-c"]
