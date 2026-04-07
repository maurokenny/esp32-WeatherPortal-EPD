#!/usr/bin/env python3
"""
json_to_header.py - Convert Open-Meteo API JSON to C++ header for ESP32

This script converts a JSON API response into a C-compatible header file
for offline testing of the weather station firmware. Used with USE_SAVED_API_DATA.

Usage:
    # Basic usage (default output path)
    python json_to_header.py api_response.json

    # Custom output path
    python json_to_header.py weather_data.json -o include/my_weather.h

Input:
    JSON file from Open-Meteo API (download with curl or browser)

Output:
    C++ header file with SAVED_API_JSON macro containing escaped JSON string

Workflow:
    1. Download Open-Meteo JSON:
       curl "https://api.open-meteo.com/v1/forecast?..." -o api_response.json
    2. Convert to header:
       python json_to_header.py api_response.json
    3. Set USE_SAVED_API_DATA 1 in config.h
    4. Rebuild firmware - skips HTTP API call (WiFi still required for NTP)

See AGENTS.md for complete documentation.
"""

import argparse
import json
import os
import sys


def escape_c_string(s):
    """Escape string for C code usage."""
    return (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def main():
    parser = argparse.ArgumentParser(
        description="Convert JSON file to C++ header for ESP32"
    )
    parser.add_argument("json_file", help="Input JSON file")
    parser.add_argument(
        "--output",
        "-o",
        default="include/saved_api_response.h",
        help="Output file (default: include/saved_api_response.h)",
    )

    args = parser.parse_args()

    # Check if input file exists
    if not os.path.isfile(args.json_file):
        print(f"Error: File '{args.json_file}' not found", file=sys.stderr)
        return 1

    # Read and validate JSON
    try:
        with open(args.json_file, "r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{args.json_file}': {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error reading file '{args.json_file}': {e}", file=sys.stderr)
        return 1

    # Convert to compact JSON
    json_compact = json.dumps(data, separators=(",", ":"), ensure_ascii=False)

    # Escape for C string
    json_escaped = escape_c_string(json_compact)

    # Generate header
    header_content = f'''#pragma once

#define SAVED_API_JSON "{json_escaped}"
'''

    # Create directory if necessary
    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    # Write file
    try:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(header_content)
        print(f"Header generated successfully: {args.output}")
        print(f"Compact JSON size: {len(json_compact)} characters")
        print(f"Escaped string size: {len(json_escaped)} characters")
    except Exception as e:
        print(f"Error writing file '{args.output}': {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
