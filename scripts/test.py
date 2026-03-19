import argparse
import subprocess
import sys
import os
import json
import shutil
from pathlib import Path

# Prevent creation of __pycache__ folders in source directories
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

# --- Constants & Paths ---
REPO_ROOT = Path(__file__).parent.parent.resolve()
TESTS_DIR = REPO_ROOT / "tests"

def print_banner(title):
    print("\n" + "="*80)
    print(f"  {title}")
    print("="*80, flush=True)

def print_step(step, title):
    print(f"\n--- [STEP {step}] {title} ---", flush=True)

# --- Stage 1: Unit Tests ---
def run_unit_tests(build_dir, config):
    is_win = sys.platform.startswith("win")
    ext = ".exe" if is_win else ""
    exe_path = build_dir / "tests" / "Unit" / (config if is_win else "") / f"UnitTests{ext}"
    
    if not exe_path.exists():
        print(f"[!] Executable not found: {exe_path}")
        return False

    print(f"Executing: {exe_path}")
    res = subprocess.run([str(exe_path), "--colour-mode", "ansi"], cwd=REPO_ROOT)
    return res.returncode == 0

# --- Stage 2: Scenario-based Tests ---
def run_scenario_tests(build_dir, config, real_lib_path=None):
    is_win = sys.platform.startswith("win")
    ext = ".exe" if is_win else ""
    librium_exe = build_dir / "apps" / "Librium" / (config if is_win else "") / f"Librium{ext}"
    
    scenario_script = REPO_ROOT / "scripts" / "ScenarioRunner.py"
    scenarios_dir = REPO_ROOT / "tests" / "Scenarios"
    output_root = build_dir / "tests" / "Scenarios"
    output_root.mkdir(parents=True, exist_ok=True)

    print(f"Executing: {scenario_script}")
    cmd = [sys.executable, "-u", str(scenario_script), str(librium_exe), str(output_root), str(scenarios_dir)]
    if real_lib_path:
        cmd.append(real_lib_path)
        
    res = subprocess.run(cmd, cwd=REPO_ROOT)
    return res.returncode == 0

# --- Stage 3: Real Library Tests ---
def run_real_library_test(build_dir, config, real_lib_path):
    is_win = sys.platform.startswith("win")
    ext = ".exe" if is_win else ""
    librium_exe = build_dir / "apps" / "Librium" / (config if is_win else "") / f"Librium{ext}"
    
    test_script = REPO_ROOT / "scripts" / "RealLibraryTest.py"
    output_root = build_dir / "tests" / "RealLibrary"
    output_root.mkdir(parents=True, exist_ok=True)

    print(f"Executing: {test_script}")
    cmd = [sys.executable, "-u", str(test_script), str(librium_exe), str(output_root), real_lib_path]
    res = subprocess.run(cmd, cwd=REPO_ROOT)
    return res.returncode == 0

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", required=True)
    parser.add_argument("--stage", choices=["unit", "scenario", "real", "all"], default="all")
    parser.add_argument("--real-library", help="Path to real library")
    args = parser.parse_args()

    build_dir = REPO_ROOT / "out" / "build" / args.preset
    config = "Release" if "release" in args.preset.lower() else "Debug"

    success = True
    
    if args.stage in ["unit", "all"]:
        print_step(1, "UNIT TESTS")
        if not run_unit_tests(build_dir, config): success = False

    if success and args.stage in ["scenario", "all"]:
        print_step(2, "SCENARIO TESTS")
        if not run_scenario_tests(build_dir, config, args.real_library): success = False

    if success and args.stage in ["real", "all"] and args.real_library:
        print_step(3, "REAL LIBRARY TESTS")
        if not run_real_library_test(build_dir, config, args.real_library): success = False

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
