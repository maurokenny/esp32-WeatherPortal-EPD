import os
from os.path import join

# Access the PlatformIO build environment
Import("env")

# Define paths for the environment configuration
# Look for .env in repository root (parent of project dir)
project_dir = env["PROJECT_DIR"]
env_file = join(os.path.dirname(project_dir), ".env")

# Fallback values used if the .env file is missing or keys are not found
wifi_ssid = "Default SSID"
wifi_password = "Default Password"

# Parse the .env file if it exists in the project root
if os.path.exists(env_file):
    try:
        with open(env_file, "r") as f:
            for line in f:
                line = line.strip()
                # Process lines containing '=' and ignore comments starting with '#'
                if "=" in line and not line.startswith("#"):
                    key, value = line.split("=", 1)

                    # Clean the value by removing potential surrounding quotes or whitespace
                    value = value.strip().strip('"').strip("'")

                    if key == "WIFI_SSID":
                        wifi_ssid = value
                    elif key == "WIFI_PASSWORD":
                        wifi_password = value

        print(f"Build System: Loaded WiFi credentials from {env_file}")
    except Exception as e:
        print(f"Build System: Error reading {env_file}: {e}")
else:
    print(f"Build System: {env_file} not found. Proceeding with fallback values.")

# Inject the credentials as global C++ macros (-D)
# env.StringifyMacro is used to ensure the values are wrapped in escaped quotes.
env.Append(
    CPPDEFINES=[
        ("WIFI_SSID_VALUE", env.StringifyMacro(wifi_ssid)),
        ("WIFI_PASSWORD_VALUE", env.StringifyMacro(wifi_password)),
    ]
)
