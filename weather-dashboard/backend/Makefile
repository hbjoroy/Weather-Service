# Weather Dashboard Backend Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
LDFLAGS = -lcurl -lcjson -lmicrohttpd

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
STATICDIR = static

# Target executable
TARGET = $(BUILDDIR)/weather-dashboard-server

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Default target
all: $(TARGET)

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Create static directory
$(STATICDIR):
	mkdir -p $(STATICDIR)

# Link the executable
$(TARGET): $(OBJECTS) | $(BUILDDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)/*

# Install dependencies (Ubuntu/Debian)
deps:
	sudo apt-get update
	sudo apt-get install -y libcurl4-openssl-dev libcjson-dev libmicrohttpd-dev

# Install dependencies (CentOS/RHEL/Fedora)
deps-rpm:
	sudo dnf install -y libcurl-devel cjson-devel libmicrohttpd-devel

# Run the server
run: $(TARGET) | $(STATICDIR)
	./$(TARGET)

# Run with CORS enabled for development
dev: $(TARGET) | $(STATICDIR)
	./$(TARGET) -c -v

# Test the server
test: $(TARGET)
	@echo "Testing server endpoints..."
	@./$(TARGET) &
	@sleep 2
	@echo "Testing profile endpoint..."
	@curl -s http://localhost:3001/api/profile | jq . || echo "jq not installed"
	@echo "Stopping test server..."
	@pkill -f weather-dashboard-server || true

# Build and copy frontend
frontend: | $(STATICDIR)
	@if [ -d "../frontend/dist" ]; then \
		echo "Copying frontend build to static directory..."; \
		cp -r ../frontend/dist/* $(STATICDIR)/; \
		echo "Frontend copied successfully."; \
	else \
		echo "Frontend not built. Run 'cd ../frontend && npm run build' first."; \
	fi

# Production build
production: all frontend
	@echo "Production build complete."
	@echo "Start server with: ./$(TARGET)"

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build the server (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  deps       - Install dependencies (Ubuntu/Debian)"
	@echo "  deps-rpm   - Install dependencies (CentOS/RHEL/Fedora)"
	@echo "  run        - Build and run the server"
	@echo "  dev        - Build and run with CORS and verbose logging"
	@echo "  test       - Build and test server endpoints"
	@echo "  frontend   - Copy frontend build to static directory"
	@echo "  production - Build everything for production"
	@echo "  debug      - Build with debug symbols"
	@echo "  help       - Show this help message"

# Phony targets
.PHONY: all clean deps deps-rpm run dev test frontend production debug help

# Dependencies
$(BUILDDIR)/main.o: $(INCDIR)/dashboard_types.h $(INCDIR)/http_server.h
$(BUILDDIR)/http_server.o: $(INCDIR)/dashboard_types.h $(INCDIR)/http_server.h $(INCDIR)/weather_client.h
$(BUILDDIR)/weather_client.o: $(INCDIR)/dashboard_types.h $(INCDIR)/weather_client.h