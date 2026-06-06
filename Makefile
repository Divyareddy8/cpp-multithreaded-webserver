CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread
INCLUDES := -I include
LDFLAGS  := -pthread

SRC_DIR := src
OBJ_DIR := build/obj
BIN_DIR := build/bin
BIN     := $(BIN_DIR)/webserver

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean debug run test help

all: $(BIN)

# Create directories if they don't exist
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Compile each .cpp → .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "  CXX  $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Link all object files into the final binary
$(BIN): $(OBJS) | $(BIN_DIR)
	@echo "  LD   $@"
	$(CXX) $(LDFLAGS) $^ -o $@
	@echo ""
	@echo "  ✔  Build successful → $(BIN)"
	@echo "  Run: $(BIN) -p 8080 -t 4 -r ./www"
	@echo ""

debug: CXXFLAGS += -g -DDEBUG -O0 -fsanitize=address,undefined
debug: LDFLAGS  += -fsanitize=address,undefined
debug: $(BIN)

run: all
	$(BIN) -p 8080 -t 4 -r ./www

# Quick test with curl
test: all
	@echo "=== Starting server in background ==="
	@$(BIN) -p 9999 -r ./www &
	@sleep 0.5
	@echo ""
	@echo "=== GET / ==="
	@curl -si http://localhost:9999/ | head -20
	@echo ""
	@echo "=== GET /notfound ==="
	@curl -si http://localhost:9999/notfound | head -10
	@echo ""
	@echo "=== POST (expect 405) ==="
	@curl -si -X POST http://localhost:9999/ | head -10
	@echo ""
	@echo "=== Path traversal (expect 403) ==="
	@curl -si http://localhost:9999/../../etc/passwd | head -10
	@pkill -f "webserver -p 9999" 2>/dev/null || true
	@echo ""
	@echo "=== Tests done ==="

clean:
	@rm -rf build
	@echo "  Cleaned build artifacts."

help:
	@echo "Targets:"
	@echo "  make          Build optimised binary"
	@echo "  make debug    Build with AddressSanitizer + debug symbols"
	@echo "  make run      Build and start server on :8080"
	@echo "  make test     Build, run tests, and stop"
	@echo "  make clean    Remove build directory"
