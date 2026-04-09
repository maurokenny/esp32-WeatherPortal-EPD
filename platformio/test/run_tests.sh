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
MOCK_SERVER_URL="http://localhost:8080"

# Script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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
echo -ne "${BLUE}Waiting for mock server${NC}"
for i in {1..30}; do
    if curl -sf http://localhost:8080/health > /dev/null 2>&1; then
        echo -e "\n${GREEN}✓ Mock server ready!${NC}"
        break
    fi
    echo -ne "."
    sleep 1
done

# Check if mock server is healthy
if ! curl -sf http://localhost:8080/health > /dev/null 2>&1; then
    echo -e "\n${RED}✗ Mock server failed to start${NC}"
    docker compose logs mock-server
    docker compose down
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

TEST_RESULT=$?

# Cleanup
echo -e "${BLUE}Cleaning up...${NC}"
cd "${SCRIPT_DIR}"
docker compose down

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}All tests passed!${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Tests failed!${NC}"
    echo -e "${RED}========================================${NC}"
fi

exit $TEST_RESULT
