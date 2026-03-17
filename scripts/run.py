import argparse
import subprocess
import sys
import os
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent.resolve()

def print_banner(title):
    print("\n" + "="*80)
    print(f"  {title}")
    print("="*80, flush=True)

def run_local(args):
    print_banner(f"LIBRIUM PIPELINE [Preset: {args.preset}]")
    
    # 1. Build
    build_cmd = [sys.executable, "-u", "scripts/build.py", "--preset", args.preset]
    if args.clean: build_cmd.append("--clean")
    res = subprocess.run(build_cmd, cwd=REPO_ROOT)
    if res.returncode != 0: return False
    
    # 2. Test
    test_cmd = [sys.executable, "-u", "scripts/test.py", "--preset", args.preset]
    if args.real_library:
        test_cmd.extend(["--real-library", args.real_library])
    
    res = subprocess.run(test_cmd, cwd=REPO_ROOT)
    return res.returncode == 0

def run_docker(args):
    print_banner(f"LIBRIUM DOCKER PIPELINE [Preset: {args.preset}]")
    
    image_name = "librium-build-linux"
    dockerfile = "Dockerfile.linux"

    # 1. Build Image
    print(f"\n>>> PHASE 1: ENSURING DOCKER IMAGE IS READY")
    build_cmd = ["docker", "build", "-t", image_name, "-f", str(REPO_ROOT / dockerfile), "."]
    res = subprocess.run(build_cmd, cwd=REPO_ROOT)
    if res.returncode != 0: return False

    # 2. Run Sequence in Docker
    print(f"\n>>> PHASE 2: EXECUTING PIPELINE INSIDE LINUX")
    
    inner_cmds = []
    build_args = f"--preset {args.preset}"
    if args.clean: build_args += " --clean"
    inner_cmds.append(f"python3 scripts/build.py {build_args}")
    
    test_args = f"--preset {args.preset}"
    if args.real_library: test_args += f" --real-library /data/real_library"
    inner_cmds.append(f"python3 -u scripts/test.py {test_args}")
    
    full_cmd = " && ".join(inner_cmds)
    
    docker_run = [
        "docker", "run", "--rm", "-t",
        "-e", "PYTHONUNBUFFERED=1",
        "-v", f"{REPO_ROOT}:/app",
    ]
    if args.real_library:
        docker_run.extend(["-v", f"{Path(args.real_library).resolve()}:/data/real_library"])
    
    docker_run.extend([image_name, "bash", "-c", full_cmd])
    
    res = subprocess.run(docker_run, cwd=REPO_ROOT)
    return res.returncode == 0

def main():
    parser = argparse.ArgumentParser(description="Librium Master Runner")
    parser.add_argument("--preset", default="x64-debug", help="CMake preset to use")
    parser.add_argument("--real-library", help="Path to real library (optional)")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    args = parser.parse_args()

    if args.preset.startswith("linux"):
        success = run_docker(args)
    else:
        success = run_local(args)

    if success:
        print_banner("PIPELINE COMPLETED SUCCESSFULLY")
    else:
        print_banner("PIPELINE FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()