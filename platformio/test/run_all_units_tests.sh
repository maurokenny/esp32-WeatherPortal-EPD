#!/bin/bash
# Run all unit-specific blackbox tests
# Usage: ./run_all_units_tests.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Start mock server
echo "========================================"
echo "Starting mock server..."
echo "========================================"
cd "$SCRIPT_DIR"
docker compose up -d mock-server
sleep 3

# Check if mock server is ready
if ! curl -sf http://localhost:8080/health > /dev/null; then
    echo "ERROR: Mock server not responding"
    docker compose logs mock-server
    docker compose down
    exit 1
fi
echo "Mock server ready!"

# List of unit tests
UNIT_TESTS=(
    "blackbox-temp-c:Temperature Celsius"
    "blackbox-temp-f:Temperature Fahrenheit"
    "blackbox-temp-k:Temperature Kelvin"
    "blackbox-wind-kmh:Wind km/h"
    "blackbox-wind-mph:Wind mph"
    "blackbox-pres-hpa:Pressure hPa"
    "blackbox-pres-mmhg:Pressure mmHg"
    "blackbox-pres-inhg:Pressure inHg"
    "blackbox-pres-psi:Pressure psi"
    "blackbox-dist-km:Distance km"
    "blackbox-dist-mi:Distance miles"
    "blackbox-precip-mm:Precipitation mm"
    "blackbox-precip-cm:Precipitation cm"
    "blackbox-precip-in:Precipitation inches"
)

PASSED=0
FAILED=0
FAILED_LIST=""

echo ""
echo "========================================"
echo "Running Unit-Specific Blackbox Tests"
echo "========================================"
echo ""

for test_config in "${UNIT_TESTS[@]}"; do
    IFS=':' read -r env_name description <<< "$test_config"
    
    echo "────────────────────────────────────────"
    echo "Testing: $description"
    echo "Environment: $env_name"
    echo "────────────────────────────────────────"
    
    if pio test -e "$env_name" 2>&1; then
        echo "✓ $description PASSED"
        ((PASSED++))
    else
        echo "✗ $description FAILED"
        ((FAILED++))
        FAILED_LIST="$FAILED_LIST\n  - $description ($env_name)"
    fi
    echo ""
done

# Cleanup
echo "========================================"
echo "Cleaning up..."
echo "========================================"
docker compose down

echo ""
echo "========================================"
echo "Results: $PASSED passed, $FAILED failed"
echo "========================================"

if [ $FAILED -gt 0 ]; then
    echo -e "Failed tests:$FAILED_LIST"
    exit 1
fi

echo "All unit tests passed!"
exit 0
