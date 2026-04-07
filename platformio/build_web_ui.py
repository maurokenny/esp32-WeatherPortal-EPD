import os
import gzip
import re
import csv

Import("env")


def get_config_value(file_path, key):
    if not os.path.exists(file_path):
        return ""
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
        # Match ACTIVE const String KEY = "VALUE"; (Ignores lines starting with //)
        match = re.search(
            rf'^\s*const\s+String\s+{key}\s*=\s*"([^"]*)"', content, re.MULTILINE
        )
        if match:
            return match.group(1)
        # Match ACTIVE const char \*KEY = "VALUE";
        match = re.search(
            rf'^\s*const\s+char\s+\*{key}\s*=\s*"([^"]*)"', content, re.MULTILINE
        )
        if match:
            return match.group(1)
        # Match ACTIVE #define KEY "VALUE"
        match = re.search(rf'^\s*#define\s+{key}\s*"([^"]*)"', content, re.MULTILINE)
        if match:
            return match.group(1)
        # Match ACTIVE #define KEY_VALUE "VALUE" (for wifi_credentials)
        match = re.search(
            rf'^\s*#define\s+{key}_VALUE\s*"([^"]*)"', content, re.MULTILINE
        )
        if match:
            return match.group(1)
    return ""


def load_timezones(zones_file):
    """Load timezone entries from zones.csv file.

    Returns a list of tuples (timezone_name, posix_string).
    """
    timezones = []
    if not os.path.exists(zones_file):
        print(f"Warning: zones.csv not found at {zones_file}")
        return timezones

    try:
        with open(zones_file, "r", encoding="utf-8") as f:
            reader = csv.reader(f)
            for row in reader:
                if len(row) >= 2:
                    # Remove quotes if present
                    tz_name = row[0].strip().strip('"')
                    tz_posix = row[1].strip().strip('"')
                    if tz_name and tz_posix:
                        timezones.append((tz_name, tz_posix))
    except Exception as e:
        print(f"Error loading zones.csv: {e}")

    return timezones


def generate_timezone_options(timezones, selected_tz):
    """Generate HTML select options from timezone list.

    Format: <option value="POSIX_STRING" selected>Timezone Name</option>
    Shows the timezone name to user, sends POSIX string as value.
    """
    options = []
    for tz_name, tz_posix in timezones:
        # Escape HTML special characters
        escaped_name = (
            tz_name.replace("&", "&amp;")
            .replace('"', "&quot;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
        )
        escaped_posix = (
            tz_posix.replace("&", "&amp;")
            .replace('"', "&quot;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
        )

        # Mark as selected if matches current timezone
        selected_attr = " selected" if tz_posix == selected_tz else ""
        options.append(
            f'<option value="{escaped_posix}"{selected_attr}>{escaped_name}</option>'
        )

    return "\n                        ".join(options)


def build_web_ui(source, target, env):
    project_dir = env.get("PROJECT_DIR")
    data_dir = os.path.join(project_dir, "data")
    template_path = os.path.join(data_dir, "setup.html")
    include_dir = os.path.join(project_dir, "include")
    output_path = os.path.join(include_dir, "web_ui_data.h")
    zones_file = os.path.join(data_dir, "zones.csv")

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
        f"Baking settings into UI: City='{city}', Country='{country}', {lat}/{lon}, Timezone='{timezone}'"
    )
    print(f"Loaded {len(timezones)} timezones from zones.csv")

    with open(template_path, "r", encoding="utf-8") as f:
        html = f.read()

    # Replace placeholders
    html = html.replace("{{CITY_STRING}}", city)
    html = html.replace("{{COUNTRY_STRING}}", country)
    html = html.replace("{{LAT}}", lat)
    html = html.replace("{{LON}}", lon)

    # Timezone: if empty, show placeholder as selected
    if timezone:
        html = html.replace("{{TIMEZONE_SELECTED_EMPTY}}", "")
    else:
        html = html.replace("{{TIMEZONE_SELECTED_EMPTY}}", "selected")

    html = html.replace("{{TIMEZONE_OPTIONS}}", timezone_options)

    # GZIP compress
    compressed = gzip.compress(html.encode("utf-8"))

    # Generate header file
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("#ifndef WEB_UI_DATA_H\n#define WEB_UI_DATA_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("// Auto-generated from data/setup.html at build time\n")
        f.write("const uint8_t index_html_gz[] PROGMEM = {\n  ")
        for i, b in enumerate(compressed):
            f.write(f"0x{b:02x}")
            if i < len(compressed) - 1:
                f.write(", ")
            if (i + 1) % 12 == 0:
                f.write("\n  ")
        f.write("\n};\n")
        f.write(f"const uint32_t index_html_gz_len = {len(compressed)};\n\n")
        f.write("#endif\n")

    print(f"Successfully generated {output_path}")


# Execute immediately (pre: script runs before any build steps)
build_web_ui(None, None, env)
