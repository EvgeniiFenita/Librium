import argparse
import subprocess
import sys
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent.resolve()

def print_step(step, title):
    print(f"\n--- [BUILD STEP {step}] {title} ---", flush=True)

def run_command(cmd, cwd=None):
    # Use -u or flush to ensure we see CMake output in real-time
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"\n[!] BUILD ERROR: Command failed with exit code {result.returncode}", flush=True)
        sys.exit(result.returncode)

def main():
    parser = argparse.ArgumentParser(description="Librium Build Script")
    parser.add_argument("--preset", required=True, help="CMake preset to use")
    parser.add_argument("--clean", action="store_true", help="Remove build directory before configuring")
    args = parser.parse_args()

    build_dir = REPO_ROOT / "out" / "build" / args.preset
    config = "Release" if "release" in args.preset.lower() else "Debug"

    if args.clean and build_dir.exists():
        print(f"--- [CLEAN] Removing build directory: {build_dir}", flush=True)
        shutil.rmtree(build_dir)

    # 1. Configure
    print_step(1, f"Configuring project (Preset: {args.preset})")
    run_command(["cmake", "--preset", args.preset], cwd=REPO_ROOT)

    # 2. Build
    print_step(2, f"Compiling all targets ({config})")
    run_command(["cmake", "--build", "--preset", args.preset, "--config", config], cwd=REPO_ROOT)

if __name__ == "__main__":
    main()
