Import("env")
import os
import glob

# Find the mklittlefs tool in PlatformIO's packages
possible_paths = [
    "D:/PlatformIO/.platformio/packages/tool-mklittlefs*/mklittlefs.exe",
    "D:/PlatformIO/.platformio/tools/tool-mklittlefs/mklittlefs.exe",
]

mklittlefs_bin = None
for pattern in possible_paths:
    matches = glob.glob(pattern)
    if matches:
        mklittlefs_bin = matches[0]
        break

if mklittlefs_bin and os.path.exists(mklittlefs_bin):
    # Replace the mklittlefs command in PATH
    env.PrependENVPath("PATH", os.path.dirname(mklittlefs_bin))
    print(f"[LittleFS] Added to PATH: {os.path.dirname(mklittlefs_bin)}")
    print(f"[LittleFS] Using mklittlefs from: {mklittlefs_bin}")
else:
    print("[LittleFS] ERROR: Could not find mklittlefs.exe")
    print(f"[LittleFS] Searched patterns: {possible_paths}")
