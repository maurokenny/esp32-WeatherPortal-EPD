# load_env.py - Generate wifi_credentials.h from .env file
# This script reads credentials from a local .env file (not versioned)
# and generates a C header file with the credentials.
#
# IMPORTANT: The .env file and generated wifi_credentials.h should NEVER be committed to git.
# Both are already added to .gitignore.

import os
from os.path import join

Import("env")

project_dir = env["PROJECT_DIR"]
env_file = join(project_dir, ".env")
header_file = join(project_dir, "include", "wifi_credentials.h")

# Default fallback values
wifi_ssid = "ssid"
wifi_password = "password"

# Read from .env if it exists
if os.path.exists(env_file):
    try:
        with open(env_file, 'r') as f:
            for line in f:
                line = line.strip()
                if '=' in line and not line.startswith('#'):
                    key, value = line.split('=', 1)
                    if key == "WIFI_SSID":
                        wifi_ssid = value
                    elif key == "WIFI_PASSWORD":
                        wifi_password = value
        print(f"Loaded WiFi credentials from {env_file}")
    except Exception as e:
        print(f"Warning: Could not read {env_file}: {e}")
        print("Using fallback WiFi credentials")
else:
    print(f"Warning: {env_file} not found. Using fallback WiFi credentials.")
    print("Create a .env file with WIFI_SSID and WIFI_PASSWORD to set your credentials.")

# Escape special characters for C string literals
def escape_c_string(s):
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n')
    s = s.replace('\r', '\\r')
    s = s.replace('\t', '\\t')
    return s

ssid_escaped = escape_c_string(wifi_ssid)
pass_escaped = escape_c_string(wifi_password)

# Generate the header file
header_content = f'''/* WiFi Credentials - AUTO-GENERATED FILE - DO NOT EDIT */
/* Generated from .env file by load_env.py */
/* This file should NOT be committed to version control */

#ifndef __WIFI_CREDENTIALS_H__
#define __WIFI_CREDENTIALS_H__

#define WIFI_SSID_VALUE "{ssid_escaped}"
#define WIFI_PASSWORD_VALUE "{pass_escaped}"

#endif /* __WIFI_CREDENTIALS_H__ */
'''

try:
    with open(header_file, 'w') as f:
        f.write(header_content)
    print(f"Generated {header_file}")
except Exception as e:
    print(f"Error writing {header_file}: {e}")
