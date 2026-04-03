#!/usr/bin/env python3
"""
Script to convert JSON to C++ header for ESP32.
Generates saved_api_response.h with escaped JSON for firmware use.
"""

import argparse
import json
import os
import sys

def escape_c_string(s):
    """Escape string for C code usage."""
    return (s.replace('\\', '\\\\')
             .replace('"', '\\"')
             .replace('\n', '\\n')
             .replace('\r', '\\r')
             .replace('\t', '\\t'))

def main():
    parser = argparse.ArgumentParser(
        description='Convert JSON file to C++ header for ESP32'
    )
    parser.add_argument(
        'json_file',
        help='Input JSON file'
    )
    parser.add_argument(
        '--output',
        '-o',
        default='include/saved_api_response.h',
        help='Output file (default: include/saved_api_response.h)'
    )

    args = parser.parse_args()

    # Check if input file exists
    if not os.path.isfile(args.json_file):
        print(f"Error: File '{args.json_file}' not found", file=sys.stderr)
        return 1

    # Read and validate JSON
    try:
        with open(args.json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{args.json_file}': {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error reading file '{args.json_file}': {e}", file=sys.stderr)
        return 1

    # Convert to compact JSON
    json_compact = json.dumps(data, separators=(',', ':'), ensure_ascii=False)

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
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"Header generated successfully: {args.output}")
        print(f"Compact JSON size: {len(json_compact)} characters")
        print(f"Escaped string size: {len(json_escaped)} characters")
    except Exception as e:
        print(f"Error writing file '{args.output}': {e}", file=sys.stderr)
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())