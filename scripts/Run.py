#!/usr/bin/env python3
import sys
import os

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

import argparse
import shutil
from pathlib import Path

# Add the scripts directory to path to import internal modules
sys.path.append(str(Path(__file__).parent))
from Core import CUI, CPaths, CRunner

def build_command(args):
    """Handles the 'build' subcommand."""
    CUI.banner(f"BUILDING LIBRIUM [Preset: {args.preset}]")
    
    build_dir = CPaths.get_build_dir(args.preset)
    artifact_dir = CPaths.get_artifact_dir(args.preset)
    config = CPaths.get_config(args.preset)

    if args.clean:
        if build_dir.exists():
            CUI.info(f"Removing build directory: {build_dir}")
            shutil.rmtree(build_dir)
        if artifact_dir.exists():
            CUI.info(f"Removing artifact directory: {artifact_dir}")
            shutil.rmtree(artifact_dir)

    # 1. Configure
    CUI.step(1, f"Configuring project (Preset: {args.preset})")
    CRunner.run(["cmake", "--preset", args.preset])

    # 2. Build
    CUI.step(2, f"Compiling all targets ({config})")
    CRunner.run(["cmake", "--build", "--preset", args.preset, "--config", config])
    return True

def test_command(args):
    """Handles the 'test' subcommand."""
    CUI.banner(f"TESTING LIBRIUM [Preset: {args.preset}]")
    
    build_dir = CPaths.get_build_dir(args.preset)
    artifact_dir = CPaths.get_artifact_dir(args.preset)
    
    success = True
    
    # 1. Unit Tests
    if args.stage in ["unit", "all"]:
        CUI.step(1, "UNIT TESTS")
        exe = CPaths.get_executable_path(build_dir, "tests/Unit", "UnitTests")
        unit_dir = artifact_dir / "unit"
        unit_dir.mkdir(parents=True, exist_ok=True)
        if not exe.exists():
            CUI.error(f"Unit test executable not found: {exe}")
            success = False
        elif not CRunner.run([str(exe), "--colour-mode", "ansi"], cwd=unit_dir):
            success = False

    # 2. Scenario Tests
    if success and args.stage in ["scenario", "all"]:
        CUI.step(2, "SCENARIO TESTS")
        from ScenarioTester import CScenarioTester
        librium_exe = CPaths.get_executable_path(build_dir, "apps/Librium", "Librium")
        output_root = artifact_dir / "scenarios"
        
        runner = CScenarioTester(librium_exe, output_root, args.real_library)
        results = runner.run_all(CPaths.SCENARIOS_DIR)
        if any(r["status"] == "FAIL" for r in results):
            success = False

    # 3. Real Library Tests
    if success and args.stage in ["real", "all"] and args.real_library:
        CUI.step(3, "REAL LIBRARY TESTS")
        from SmokeTester import CSmokeTester
        librium_exe = CPaths.get_executable_path(build_dir, "apps/Librium", "Librium")
        
        # Determine if this is a synthetic run or a real library run
        is_synthetic = "synthetic_lib" in str(args.real_library)
        folder_name = "synthetic_run" if is_synthetic else "real_run"
        output_root = artifact_dir / folder_name
        
        test = CSmokeTester(librium_exe, output_root, args.real_library)
        if not test.run():
            success = False

    return success

def gen_command(args):
    """Handles the 'gen' subcommand."""
    # Run LibraryGenerator.py as a script
    CRunner.run_python("LibraryGenerator.py", sys.argv[2:])
    return True

def run_pipeline(args):
    """Executes the full sequence: Build -> Unit/Scenario Tests -> Synthetic Smoke Test (if no real lib)."""
    preset = args.preset or "x64-debug"
    
    # 1. Build
    class FakeArgs: pass
    b_args = FakeArgs()
    b_args.preset = preset
    b_args.clean = args.clean
    if not build_command(b_args): return False
    
    # 2. Basic Tests (Unit + Scenario)
    t_args = FakeArgs()
    t_args.preset = preset
    t_args.stage = "all"
    t_args.real_library = args.real_library
    if not test_command(t_args): return False
    
    # 3. Synthetic Smoke Test (only if no real library provided)
    if not args.real_library:
        CUI.step(4, "GENERATING SYNTHETIC 'REAL' LIBRARY FOR SMOKE TEST")
        lib_dir = CPaths.get_artifact_dir(preset) / "synthetic_lib"
        
        if lib_dir.exists():
            shutil.rmtree(lib_dir)
        
        if not CRunner.run_python("Run.py", ["gen", "--out", str(lib_dir), "--books", "100", "--archive-size", "10", "--covers"]):
            return False
        
        CUI.step(5, "RUNNING SMOKE TEST ON SYNTHETIC LIBRARY")
        t_args.real_library = str(lib_dir / "test_library")
        t_args.stage = "real"
        if not test_command(t_args): return False
        
    return True

def run_docker(args):
    CUI.banner(f"LIBRIUM DOCKER PIPELINE [Preset: {args.preset}]")
    
    image_name = "librium-build-linux"
    dockerfile = "Dockerfile.linux"

    # 1. Build Image
    CUI.info("PHASE 1: ENSURING DOCKER IMAGE IS READY")
    CRunner.run(["docker", "build", "-t", image_name, "-f", str(CPaths.REPO_ROOT / dockerfile), "."])

    # 2. Run Sequence in Docker
    CUI.info("PHASE 2: EXECUTING PIPELINE INSIDE LINUX")
    
    inner_cmd = f"python3 -u scripts/Run.py --preset {args.preset}"
    if args.clean: inner_cmd += " --clean"
    if args.real_library: inner_cmd += " --real-library /data/real_library"
    
    docker_run = [
        "docker", "run", "--rm", "-t",
        "-e", "PYTHONUNBUFFERED=1",
        "-e", "LIBRIUM_IN_DOCKER=1",
        "-v", f"{CPaths.REPO_ROOT}:/app",
    ]
    if args.real_library:
        docker_run.extend(["-v", f"{Path(args.real_library).resolve()}:/data/real_library"])
    
    docker_run.extend([image_name, "bash", "-c", inner_cmd])
    
    return CRunner.run(docker_run)

def main():
    parser = argparse.ArgumentParser(description="Librium Unified CLI")
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")

    # Build Subcommand
    build_p = subparsers.add_parser("build", help="Build the project")
    build_p.add_argument("--preset", default="x64-debug", help="CMake preset to use")
    build_p.add_argument("--clean", action="store_true", help="Clean build directory")

    # Test Subcommand
    test_p = subparsers.add_parser("test", help="Run tests")
    test_p.add_argument("--preset", default="x64-debug", help="CMake preset to use")
    test_p.add_argument("--stage", choices=["unit", "scenario", "real", "all"], default="all")
    test_p.add_argument("--real-library", help="Path to real library")

    # Gen Subcommand
    gen_p = subparsers.add_parser("gen", help="Generate test library")

    # Backward compatibility / Pipeline mode
    parser.add_argument("--preset", help="CMake preset to use (legacy mode)")
    parser.add_argument("--real-library", help="Path to real library (legacy mode)")
    parser.add_argument("--clean", action="store_true", help="Clean build directory (legacy mode)")

    args, unknown = parser.parse_known_args()
    preset = args.preset or "x64-debug"

    if args.command == "build":
        success = build_command(args)
    elif args.command == "test":
        success = test_command(args)
    elif args.command == "gen":
        success = gen_command(args)
    else:
        # Legacy / Pipeline mode
        if preset.startswith("linux") and not os.environ.get("LIBRIUM_IN_DOCKER"):
            success = run_docker(args)
        else:
            success = run_pipeline(args)

    if success:
        CUI.banner("COMPLETED SUCCESSFULLY")
    else:
        CUI.banner("FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
