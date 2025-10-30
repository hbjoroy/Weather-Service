# Multi-stage build for Weather Service
# Supports both amd64 and arm64 architectures via QEMU emulation
FROM gcc:12 AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    libcjson-dev \
    libmicrohttpd-dev \
    libssl-dev \
    make \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY src/ src/
COPY include/ include/
COPY Makefile .

# Build the application
RUN make clean && make

# Runtime stage
FROM debian:12-slim

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libcurl4 \
    libcjson1 \
    libmicrohttpd12 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -r -u 1000 -g root weather && \
    mkdir -p /app && \
    chown -R weather:root /app

WORKDIR /app

# Copy built binary from builder stage
COPY --from=builder /build/build/weather_service /app/weather_service

# Switch to non-root user
USER weather

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["/app/weather_service", "--help"] || exit 1

# Set default environment variables
ENV WEATHERAPI_KEY="" \
    SLACK_BOT_TOKEN="" \
    SLACK_APP_ID="" \
    PORT=8080

# Run the service
CMD ["/app/weather_service", "-s", "-p", "8080", "-b", "0.0.0.0", "-v", "-C"]
