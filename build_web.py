Import("env")
import gzip
import os
import shutil
import subprocess
import sys
from SCons.Script import COMMAND_LINE_TARGETS

def build_and_gzip_web(*args, **kwargs):
    """Build Svelte app and gzip output for filesystem upload"""
    print("[Web] Starting web build process...")

    project_dir = env.get("PROJECT_DIR")
    web_src_dir = os.path.join(project_dir, "web_src")
    dest_dir = os.path.join(project_dir, "data", "web")

    # Any failure below halts the SCons run via sys.exit(1) rather than `return`,
    # otherwise a vite syntax error would silently leave data/web/ empty and produce
    # a working firmware image with no UI assets.
    def fail(msg):
        print(f"[Web] {msg}")
        sys.exit(1)

    if not os.path.exists(web_src_dir):
        print("[Web] No web_src directory found, skipping")
        return

    try:
        subprocess.run(["npm", "--version"], capture_output=True, check=True, shell=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        fail("npm not found — required for filesystem builds")

    node_modules = os.path.join(web_src_dir, "node_modules")
    if not os.path.exists(node_modules):
        print("[Web] Running npm install...")
        result = subprocess.run(
            ["npm", "install"],
            cwd=web_src_dir,
            shell=True,
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"[Web] npm install stderr: {result.stderr}")
            print(f"[Web] npm install stdout: {result.stdout}")
            fail("npm install failed")

    if os.path.exists(dest_dir):
        print(f"[Web] Cleaning {dest_dir}...")
        shutil.rmtree(dest_dir)

    print("[Web] Building Svelte app...")
    result = subprocess.run(
        ["npm", "run", "build"],
        cwd=web_src_dir,
        shell=True,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"[Web] stderr:\n{result.stderr}")
        print(f"[Web] stdout:\n{result.stdout}")
        fail("vite build failed — fix the Svelte/JS error above before re-running")

    print("[Web] Build complete, gzipping assets...")

    extensions_to_gzip = ('.html', '.js', '.css')
    count = 0

    for root, dirs, files in os.walk(dest_dir):
        for file in files:
            file_path = os.path.join(root, file)

            if file.endswith('.gz'):
                continue

            if file.endswith(extensions_to_gzip):
                src_size = os.path.getsize(file_path)
                gz_path = file_path + '.gz'

                with open(file_path, 'rb') as f_in:
                    with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
                        shutil.copyfileobj(f_in, f_out)

                gz_size = os.path.getsize(gz_path)
                ratio = (1 - gz_size / src_size) * 100 if src_size > 0 else 0
                print(f"[Gzip] {file}: {src_size} -> {gz_size} bytes ({ratio:.0f}% smaller)")

                os.remove(file_path)
                count += 1

    print(f"[Web] Compressed {count} files to data/web/")

# Run web build EAGERLY at script-load time when an fs-related target is requested.
# Why eager: the LittleFS image is generated as part of the buildfs/uploadfs flow
# from data/. An AddPreAction would fire too late on some PlatformIO versions,
# leaving the device flashed with the previous bundle.
_FS_TARGETS = {"buildfs", "uploadfs"}
if _FS_TARGETS.intersection(COMMAND_LINE_TARGETS):
    build_and_gzip_web()
