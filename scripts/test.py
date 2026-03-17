import argparse
import subprocess
import sys
import os
import json
import shutil
from pathlib import Path

# --- Constants & Paths ---
REPO_ROOT = Path(__file__).parent.parent.resolve()
TESTS_DIR = REPO_ROOT / "tests"
REAL_LIB_SRC = TESTS_DIR / "RealLibrary"

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

# --- Stage 2: Integration Tests ---
def run_integration_tests(build_dir, config):
    is_win = sys.platform.startswith("win")
    ext = ".exe" if is_win else ""
    librium_exe = build_dir / "apps" / "Librium" / (config if is_win else "") / f"Librium{ext}"
    
    int_script = TESTS_DIR / "Integration" / "RunIntegrationTests.py"
    data_dir = build_dir / "tests" / "Integration" / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    print(f"Executing: {int_script}")
    cmd = [sys.executable, "-u", str(int_script), "--librium", str(librium_exe), "--data-dir", str(data_dir)]
    res = subprocess.run(cmd, cwd=REPO_ROOT)
    return res.returncode == 0

# --- Stage 3: Real Library Tests ---
def run_real_library_tests(build_dir, config, preset, real_lib_path=None):
    is_win = sys.platform.startswith("win")
    ext = ".exe" if is_win else ""
    librium_exe = build_dir / "apps" / "Librium" / (config if is_win else "") / f"Librium{ext}"
    artifact_dir = build_dir / "tests" / "RealLibrary" / "artifacts"
    
    if artifact_dir.exists():
        try:
            shutil.rmtree(artifact_dir)
        except Exception as e:
            print(f"[WARN] Could not clean artifact directory: {e}")
            print("[WARN] Please ensure no other programs (like SQLite Browser) are using the database files.")
    
    artifact_dir.mkdir(parents=True, exist_ok=True)

    # 1. Prepare adapted configs in artifact dir
    for cfg_file in REAL_LIB_SRC.glob("*.json"):
        with open(cfg_file, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        
        if real_lib_path:
            if "library" in cfg:
                lib_base = "/data/real_library" if not is_win else real_lib_path
                cfg["library"]["inpxPath"] = str(Path(lib_base) / "librusec_local_fb2.inpx")
                cfg["library"]["archivesDir"] = str(Path(lib_base) / "lib.rus.ec")
        
        if "database" in cfg: cfg["database"]["path"] = str(artifact_dir / Path(cfg["database"]["path"]).name)
        if "logging" in cfg: cfg["logging"]["file"] = str(artifact_dir / Path(cfg["logging"]["file"]).name)

        with open(artifact_dir / cfg_file.name, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)

    # 2. Run sub-scripts
    scripts = ["01_TestImport.py", "02_SearchQuality.py", "03_CheckLogs.py", "04_TestFiltering.py", "05_TestExport.py", "06_TestFb2Parsing.py"]
    
    os.environ["LIBRIUM_EXE"] = str(librium_exe)
    os.environ["LIBRIUM_ARTIFACT_DIR"] = str(artifact_dir)
    
    for i, s in enumerate(scripts, 1):
        print(f"\n[{i}/{len(scripts)}] RUNNING: {s}", flush=True)
        res = subprocess.run([sys.executable, "-u", str(REAL_LIB_SRC / s)], env=os.environ)
        if res.returncode != 0:
            print(f"[!] FAILED: {s}")
            return False
    return True

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", required=True)
    parser.add_argument("--stage", choices=["unit", "integration", "real", "all"], default="all")
    parser.add_argument("--real-library", help="Path to real library")
    args = parser.parse_args()

    build_dir = REPO_ROOT / "out" / "build" / args.preset
    config = "Release" if "release" in args.preset.lower() else "Debug"

    success = True
    
    if args.stage in ["unit", "all"]:
        print_step(1, "UNIT TESTS")
        if not run_unit_tests(build_dir, config): success = False

    if success and args.stage in ["integration", "all"]:
        print_step(2, "INTEGRATION TESTS")
        if not run_integration_tests(build_dir, config): success = False

    if success and (args.stage == "real" or (args.stage == "all" and args.real_library)):
        print_step(3, "REAL LIBRARY TESTS")
        if not run_real_library_tests(build_dir, config, args.preset, args.real_library): success = False

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
