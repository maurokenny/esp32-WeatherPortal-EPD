#!/bin/bash
# Blackbox Integration Test Runner
#
# This script runs the blackbox integration tests using docker-compose
# or directly with PlatformIO.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default settings
VERBOSE=false
MOCK_SERVER_HOST="${MOCK_SERVER_HOST:-localhost}"
MOCK_SERVER_PORT="${MOCK_SERVER_PORT:-8080}"
MOCK_SERVER_STARTUP_RETRIES="${MOCK_SERVER_STARTUP_RETRIES:-30}"
MOCK_URL="http://${MOCK_SERVER_HOST}:${MOCK_SERVER_PORT}"

# Script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Cleanup function
cleanup() {
    local exit_code=$?
    echo -e "\n${BLUE}Cleaning up resources...${NC}"
    cd "${SCRIPT_DIR}"
    docker compose down --remove-orphans || true
    
    # Hide cleanup messages if it was just help requested
    if [ "$1" == "help" ]; then
        exit 0
    fi
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}========================================${NC}"
        echo -e "${GREEN}All tests passed!${NC}"
        echo -e "${GREEN}========================================${NC}"
    else
        echo -e "${RED}========================================${NC}"
        echo -e "${RED}Tests failed!${NC}"
        echo -e "${RED}========================================${NC}"
    fi
    exit $exit_code
}

trap cleanup EXIT

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "  --verbose, -v   Enable verbose output"
            trap - EXIT # Remove trap so we don't print cleanup message for help
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Weather EPD Blackbox Test Runner${NC}"
echo -e "${BLUE}========================================${NC}"

# Check prerequisites
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Docker is not installed!${NC}"
    exit 1
fi

if ! command -v docker compose &> /dev/null; then
    echo -e "${RED}Docker Compose is not installed!${NC}"
    exit 1
fi

cd "${SCRIPT_DIR}"

# Build and start mock server
echo -e "${CYAN}Starting mock server with Docker Compose...${NC}"
docker compose build mock-server
docker compose up -d mock-server

# Wait for mock server
echo -e "${BLUE}Waiting for mock server at ${MOCK_URL}/health...${NC}"
SERVER_READY=false
for attempt in $(seq 1 $MOCK_SERVER_STARTUP_RETRIES); do
    if curl -sf --connect-timeout 2 --max-time 5 "${MOCK_URL}/health" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Mock server ready after ${attempt}s${NC}"
        SERVER_READY=true
        break
    fi
    echo -ne "."
    sleep 1
done

echo "" # Newline after dots

# Check if mock server is healthy
if [ "$SERVER_READY" = false ]; then
    echo -e "${RED}✗ Mock server failed to start after ${MOCK_SERVER_STARTUP_RETRIES}s${NC}"
    echo -e "${YELLOW}Docker logs:${NC}"
    docker compose logs mock-server
    exit 1
fi

# Check PlatformIO
if ! command -v pio &> /dev/null; then
    echo -e "${YELLOW}Installing PlatformIO...${NC}"
    python3 -m pip install -U platformio
fi

# Run tests
echo -e "${BLUE}Running PlatformIO blackbox tests...${NC}"
cd "${SCRIPT_DIR}/.."
if [ "$VERBOSE" = true ]; then
    pio test -e blackbox --verbose
else
    pio test -e blackbox
fi

# The exit process will trigger the trap properly cleaning up the docker resources
