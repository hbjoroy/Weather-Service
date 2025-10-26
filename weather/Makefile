CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lcurl -lcjson -lmicrohttpd # Test target
test: $(TARGET)
	@if [ -z "$$WEATHERAPI_KEY" ]; then \
		echo "Error: WEATHERAPI_KEY environment variable not set"; \
		echo "Set it with: export WEATHERAPI_KEY=your_api_key"; \
		exit 1; \
	fi
	@echo "Testing weather service with London..."
	./$(TARGET) "London"

# Test forecast target
test-forecast: $(TARGET)
	@if [ -z "$$WEATHERAPI_KEY" ]; then \
		echo "Error: WEATHERAPI_KEY environment variable not set"; \
		echo "Set it with: export WEATHERAPI_KEY=your_api_key"; \
		exit 1; \
	fi
	@echo "Testing 3-day forecast for London..."
	./$(TARGET) -f3 "London"

# Test server mode (background)
test-server: $(TARGET)
	@if [ -z "$$WEATHERAPI_KEY" ]; then \
		echo "Error: WEATHERAPI_KEY environment variable not set"; \
		echo "Set it with: export WEATHERAPI_KEY=your_api_key"; \
		exit 1; \
	fi
	@echo "Starting weather service in server mode on port 8080..."
	@echo "Press Ctrl+C to stop the server"
	./$(TARGET) -s -p 8080tpd

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
LIBDIR = lib

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Target executable
TARGET = $(BUILDDIR)/weather_service

# Include directories
INCLUDES = -I$(INCDIR)

# Default target
all: $(TARGET)

# Create build directory if it doesn't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

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

# Run the program
run: $(TARGET)
	./$(TARGET)

# Debug version
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build the weather service"
	@echo "  clean         - Remove build artifacts"
	@echo "  deps          - Install dependencies (Debian/Ubuntu)"
	@echo "  deps-rpm      - Install dependencies (CentOS/RHEL/Fedora)"
	@echo "  run           - Build and run the program"
	@echo "  debug         - Build debug version"
	@echo "  test          - Test with London current weather (requires WEATHERAPI_KEY)"
	@echo "  test-forecast - Test with 3-day forecast (requires WEATHERAPI_KEY)"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Environment Variables:"
	@echo "  WEATHERAPI_KEY - Your WeatherAPI.com API key"
	@echo "                   Get one free at https://www.weatherapi.com/"

# Test target
test: $(TARGET)
	@if [ -z "$$WEATHERAPI_KEY" ]; then \
		echo "Error: WEATHERAPI_KEY environment variable not set"; \
		echo "Set it with: export WEATHERAPI_KEY=your_api_key"; \
		exit 1; \
	fi
	@echo "Testing weather service with London current weather..."
	./$(TARGET) "London"

# Test forecast
test-forecast: $(TARGET)
	@if [ -z "$$WEATHERAPI_KEY" ]; then \
		echo "Error: WEATHERAPI_KEY environment variable not set"; \
		echo "Set it with: export WEATHERAPI_KEY=your_api_key"; \
		exit 1; \
	fi
	@echo "Testing 3-day forecast for London..."
	./$(TARGET) -f3 "London"

.PHONY: all clean deps deps-rpm run debug help test test-forecast