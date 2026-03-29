import os
import gzip
import re

Import("env")


def get_config_value(file_path, key):
    if not os.path.exists(file_path):
        return ""
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
        # Match ACTIVE const String KEY = "VALUE"; (Ignores lines starting with //)
        match = re.search(rf'^\s*const\s+String\s+{key}\s*=\s*"([^"]*)"', content, re.MULTILINE)
        if match:
            return match.group(1)
        # Match ACTIVE #define KEY "VALUE"
        match = re.search(rf'^\s*#define\s+{key}\s*"([^"]*)"', content, re.MULTILINE)
        if match:
            return match.group(1)
        # Match ACTIVE #define KEY_VALUE "VALUE" (for wifi_credentials)
        match = re.search(rf'^\s*#define\s+{key}_VALUE\s*"([^"]*)"', content, re.MULTILINE)
        if match:
            return match.group(1)
    return ""


def build_web_ui(source, target, env):
    data_dir = os.path.join(env.get("PROJECT_DIR"), "data")
    template_path = os.path.join(data_dir, "setup.html")
    include_dir = os.path.join(env.get("PROJECT_DIR"), "include")
    output_path = os.path.join(include_dir, "web_ui_data.h")

    config_cpp = os.path.join(env.get("PROJECT_DIR"), "src", "config.cpp")

    if not os.path.exists(template_path):
        print(f"Warning: template {template_path} not found!")
        return

    # Extract current values

    city = get_config_value(config_cpp, "CITY_STRING")
    country = get_config_value(config_cpp, "COUNTRY_STRING")
    lat = get_config_value(config_cpp, "LAT")
    lon = get_config_value(config_cpp, "LON")

    print(f"Baking settings into UI: City='{city}', Country='{country}', {lat}/{lon}")

    with open(template_path, "r", encoding="utf-8") as f:
        html = f.read()

    # Replace placeholders
    html = html.replace("{{CITY_STRING}}", city)
    html = html.replace("{{COUNTRY_STRING}}", country)
    html = html.replace("{{LAT}}", lat)
    html = html.replace("{{LON}}", lon)

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


# Add the build step
env.AddPreAction("buildprog", build_web_ui)
env.AddPreAction("checkprog", build_web_ui)  # For IDE checks
