#!/bin/bash
set -uo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
while [ ! -f "$ROOT_DIR/platformio.ini" ]; do
  if [ "$ROOT_DIR" = "/" ]; then
    echo "PlatformIO ini not found" >&2; exit 1
  fi
  ROOT_DIR=$(dirname "$ROOT_DIR")
done
cd "$ROOT_DIR" || exit 1

# Cleanup function to always remove docker containers cleanly
cleanup() {
    local exit_code=$?
    echo -e "\n${BLUE}Cleaning up resources...${NC}"
    cd "${SCRIPT_DIR}"
    docker compose down --remove-orphans || true
    
    if [ $exit_code -ne 0 ] || [ "${failed:-0}" -ne 0 ]; then 
        echo -e "${RED}========================================${NC}"
        echo -e "${RED}Tests failed or were interrupted!${NC}"
        echo -e "${RED}========================================${NC}"
        [ $exit_code -eq 0 ] && exit 1 || exit $exit_code
    else
        echo -e "${GREEN}========================================${NC}"
        echo -e "${GREEN}All unit tests passed!${NC}"
        echo -e "${GREEN}========================================${NC}"
        exit 0
    fi
}

trap cleanup EXIT

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Unit Tests (Matrix) Runner - Verbose${NC}"
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

# Build and start mock server
echo -e "${CYAN}Starting mock server with Docker Compose...${NC}"
cd "$SCRIPT_DIR"
docker compose build mock-server
docker compose up -d mock-server

# Wait for mock server
echo -ne "${BLUE}Waiting for mock server${NC}"
for i in {1..30}; do
    if curl -sf http://localhost:8080/health > /dev/null 2>&1; then
        echo -e "\n${GREEN}Mock server ready!${NC}"
        break
    fi
    echo -ne "."
    sleep 1
done

if ! curl -sf http://localhost:8080/health > /dev/null 2>&1; then
    echo -e "\n${RED}Mock server failed to start${NC}"
    docker compose logs mock-server
    docker compose down
    exit 1
fi

cd "$ROOT_DIR"

ENVIRONMENTS=(
  "blackbox-temp-c"  
  "blackbox-temp-f" 
  "blackbox-temp-k" 
  "blackbox-wind-kmh" 
  "blackbox-wind-mph" 
  "blackbox-pres-hpa" 
  "blackbox-pres-mmhg"
  "blackbox-pres-inhg" 
  "blackbox-pres-psi" 
  "blackbox-dist-km" 
  "blackbox-dist-mi" 
  "blackbox-precip-mm" 
  "blackbox-precip-cm" 
  "blackbox-precip-in" 
)

passed=0
failed=0
FAILED_ENVS=()

echo -e "\n${CYAN}Running all test environments...${NC}\n"

for env in "${ENVIRONMENTS[@]}"; do
  echo -e "${BLUE}----------------------------------------${NC}"
  echo -e "${CYAN}[$(($passed + $failed + 1))/$((${#ENVIRONMENTS[@]}))] Testing: ${YELLOW}$env${NC}"
  echo -e "${BLUE}----------------------------------------${NC}"
  
  if pio test -e "$env" --verbose; then
    echo -e "${GREEN}  ✓ [OK] $env${NC}\n"
    ((passed++))
  else
    echo -e "${RED}  ✗ [FAILED] $env${NC}\n"
    FAILED_ENVS+=("$env")
    ((failed++))
  fi
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}           FINAL SUMMARY                ${NC}"
echo -e "${BLUE}========================================${NC}"
printf "${GREEN}Passed:  %d${NC}\n" "$passed"
printf "${RED}Failed:  %d${NC}\n" "$failed"
echo ""

if [ ${#FAILED_ENVS[@]} -gt 0 ]; then
  echo -e "${RED}Failed environments:${NC}"
  for env in "${FAILED_ENVS[@]}"; do
    echo -e "  ${RED}✗ $env${NC}"
  done
  echo ""
fi

# The exit process will trigger the trap properly cleaning up the docker resources
if [ "$failed" -ne 0 ]; then
    exit 1
fi
exit 0
