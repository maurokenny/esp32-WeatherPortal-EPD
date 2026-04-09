#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
while [ ! -f "$ROOT_DIR/platformio.ini" ]; do
  if [ "$ROOT_DIR" = "/" ]; then
    echo "PlatformIO ini not found" >&2; exit 1
  fi
  ROOT_DIR=$(dirname "$ROOT_DIR")
done
cd "$ROOT_DIR" || exit 1

echo "Running unit tests (matrix) from root: $ROOT_DIR"

ENVIRONMENTS=(
  "blackbox-temp-c"  
  "blackbox-temp-f" 
  "blackbox-temp-k" 
  "blackbox-wind-kmh" 
  "blackbox-wind-mph" 
  "blackbox-pres-hpa" 
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
for env in "${ENVIRONMENTS[@]}"; do
  echo "Testing $env..."
  if pio test -e "$env"; then
    echo "  [OK] $env"
    ((passed++))
  else
    echo "  [ERR] $env"
    ((failed++))
  fi
done

echo
printf "Passed: %d; Failed: %d\n" "$passed" "$failed"
if [ "$failed" -ne 0 ]; then exit 1; fi
