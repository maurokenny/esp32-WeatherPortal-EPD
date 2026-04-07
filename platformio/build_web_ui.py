"""Build script to compress and embed web UI as C header.

Compresses data/setup.html with gzip and generates include/web_ui_data.h
containing the compressed bytes as a PROGMEM array.
"""

import gzip
import html
import os
import re
from csv import reader

Import("env")


def get_config_value(file_path, key):
    """Extract string value from C++ source by key name."""
    if not os.path.exists(file_path):
        return ""

    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Match: const String KEY = "VALUE";
    #        const char *KEY = "VALUE";
    #        #define KEY "VALUE"
    #        #define KEY_VALUE "VALUE"
    patterns = [
        rf'^\s*const\s+String\s+{key}\s*=\s*"([^"]*)"',
        rf'^\s*const\s+char\s+\*{key}\s*=\s*"([^"]*)"',
        rf'^\s*#define\s+{key}\s*"([^"]*)"',
        rf'^\s*#define\s+{key}_VALUE\s*"([^"]*)"',
    ]

    for pattern in patterns:
        match = re.search(pattern, content, re.MULTILINE)
        if match:
            return match.group(1)

    return ""


def load_timezones(zones_file):
    """Load timezone entries from zones.csv file."""
    if not os.path.exists(zones_file):
        print(f"Warning: zones.csv not found at {zones_file}")
        return []

    try:
        with open(zones_file, "r", encoding="utf-8") as f:
            return [
                (row[0].strip().strip('"'), row[1].strip().strip('"'))
                for row in reader(f)
                if len(row) >= 2 and row[0].strip() and row[1].strip()
            ]
    except Exception as e:
        print(f"Error loading zones.csv: {e}")
        return []


def generate_timezone_options(timezones, selected_tz):
    """Generate HTML select options from timezone list."""
    options = []
    for tz_name, tz_posix in timezones:
        selected = " selected" if tz_posix == selected_tz else ""
        options.append(
            f'<option value="{html.escape(tz_posix)}"{selected}>'
            f'{html.escape(tz_name)}</option>'
        )
    return "\n                        ".join(options)


def format_c_array(data, bytes_per_line=12):
    """Format binary data as C-style hex array."""
    hex_bytes = [f"0x{b:02x}" for b in data]
    lines = [
        ", ".join(hex_bytes[i : i + bytes_per_line]) + ","
        for i in range(0, len(hex_bytes), bytes_per_line)
    ]
    return "\n  ".join(lines)


def build_web_ui(source, target, env):
    """Generate compressed web UI header from template."""
    project_dir = env.get("PROJECT_DIR")
    data_dir = os.path.join(project_dir, "data")
    include_dir = os.path.join(project_dir, "include")

    template_path = os.path.join(data_dir, "setup.html")
    zones_file = os.path.join(data_dir, "zones.csv")
    output_path = os.path.join(include_dir, "web_ui_data.h")
    config_cpp = os.path.join(project_dir, "src", "config.cpp")

    if not os.path.exists(template_path):
        print(f"Warning: template {template_path} not found!")
        return

    # Extract current values from config.cpp
    city = get_config_value(config_cpp, "CITY_STRING")
    country = get_config_value(config_cpp, "COUNTRY_STRING")
    lat = get_config_value(config_cpp, "LAT")
    lon = get_config_value(config_cpp, "LON")
    timezone = get_config_value(config_cpp, "TIMEZONE")

    # Load timezone options from zones.csv
    timezones = load_timezones(zones_file)
    timezone_options = generate_timezone_options(timezones, timezone)

    print(
        f"Baking settings into UI: City='{city}', Country='{country}', "
        f"{lat}/{lon}, Timezone='{timezone}'"
    )
    print(f"Loaded {len(timezones)} timezones from zones.csv")

    # Load and process template
    with open(template_path, "r", encoding="utf-8") as f:
        html_content = f.read()

    html_content = html_content.replace("{{CITY_STRING}}", city)
    html_content = html_content.replace("{{COUNTRY_STRING}}", country)
    html_content = html_content.replace("{{LAT}}", lat)
    html_content = html_content.replace("{{LON}}", lon)
    html_content = html_content.replace(
        "{{TIMEZONE_SELECTED_EMPTY}}", "" if timezone else "selected"
    )
    html_content = html_content.replace("{{TIMEZONE_OPTIONS}}", timezone_options)

    # Compress and generate header
    compressed = gzip.compress(html_content.encode("utf-8"))

    header_content = f"""#ifndef WEB_UI_DATA_H
#define WEB_UI_DATA_H

#include <Arduino.h>

// Auto-generated from data/setup.html at build time
const uint8_t index_html_gz[] PROGMEM = {{
  {format_c_array(compressed)}
}};
const uint32_t index_html_gz_len = {len(compressed)};

#endif
"""

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(header_content)

    print(f"Successfully generated {output_path}")


# Execute immediately (pre: script runs before any build steps)
build_web_ui(None, None, env)
