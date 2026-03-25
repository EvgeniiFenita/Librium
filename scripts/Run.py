#!/usr/bin/env python3
import sys
import os

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

import argparse
import shutil
import json
import subprocess
from pathlib import Path

sys.path.append(str(Path(__file__).parent))
from Core import CUI, CPaths, CRunner

# ---------------------------------------------------------------------------
# Help examples appended to --help output
# ---------------------------------------------------------------------------
_HELP_EPILOG = """
examples:
  # Full CI pipeline: build + unit tests + scenario tests + web API tests
  python Run.py --preset x64-debug

  # CI pipeline with clean build
  python Run.py --preset x64-release --clean

  # Linux CI via Docker (unit + scenario only; web tests are skipped)
  python Run.py --preset linux-debug

  # Smoke test against a real library
  python Run.py --preset x64-release --library "D:/lib.rus.ec"

  # Start web server with an auto-generated demo library (~350 books)
  python Run.py --preset x64-debug --web --demo

  # Start web server against a real library
  python Run.py --preset x64-release --web --library "D:/lib.rus.ec"

  # Generate synthetic library for manual inspection (advanced)
  python Run.py gen --out ./tmp/mylib --books 500 --archive-size 50 --covers
"""

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def _do_build(preset: str, clean: bool) -> bool:
    """Configure and compile the project for the given preset."""
    CUI.banner(f"BUILDING LIBRIUM [Preset: {preset}]")

    build_dir = CPaths.get_build_dir(preset)
    artifact_dir = CPaths.get_artifact_dir(preset)
    config = CPaths.get_config(preset)

    if clean:
        if build_dir.exists():
            CUI.info(f"Removing build directory: {build_dir}")
            shutil.rmtree(build_dir)
        if artifact_dir.exists():
            CUI.info(f"Removing artifact directory: {artifact_dir}")
            shutil.rmtree(artifact_dir)

    CUI.step(1, f"Configuring project (Preset: {preset})")
    if not CRunner.run(["cmake", "--preset", preset], exit_on_fail=False):
        CUI.error("CMake configuration failed.")
        return False

    CUI.step(2, f"Compiling all targets ({config})")
    if not CRunner.run(
        ["cmake", "--build", "--preset", preset, "--config", config],
        exit_on_fail=False,
    ):
        CUI.error("CMake build failed.")
        return False

    return True


# ---------------------------------------------------------------------------
# Test stages
# ---------------------------------------------------------------------------

def _do_unit_tests(preset: str) -> bool:
    """Run the Catch2 unit test binary."""
    CUI.step(1, "UNIT TESTS")

    build_dir = CPaths.get_build_dir(preset)
    artifact_dir = CPaths.get_artifact_dir(preset)
    exe = CPaths.get_executable_path(build_dir, "tests/Unit", "UnitTests")
    unit_dir = artifact_dir / "unit"
    unit_dir.mkdir(parents=True, exist_ok=True)

    if not exe.exists():
        CUI.error(f"Unit test executable not found: {exe}")
        return False

    return CRunner.run([str(exe), "--colour-mode", "ansi"], cwd=unit_dir, exit_on_fail=False)


def _do_scenario_tests(preset: str, library_path: str = None) -> bool:
    """Run the JSON-driven scenario test suite."""
    CUI.step(2, "SCENARIO TESTS")

    from ScenarioTester import CScenarioTester

    build_dir = CPaths.get_build_dir(preset)
    artifact_dir = CPaths.get_artifact_dir(preset)
    librium_exe = CPaths.get_executable_path(build_dir, "apps/Librium", "Librium")
    output_root = artifact_dir / "scenarios"

    runner = CScenarioTester(librium_exe, output_root, library_path)
    results = runner.run_all(CPaths.SCENARIOS_DIR)
    return not any(r["status"] == "FAIL" for r in results)


def _do_web_tests(preset: str) -> bool:
    """
    Run Jest/Supertest web API tests in an isolated artifact directory.
    The web/ source directory is never modified; node_modules go to out/.
    """
    CUI.step(3, "WEB API TESTS")

    web_src = CPaths.REPO_ROOT / "web"
    artifact_dir = CPaths.get_artifact_dir(preset)
    web_test_root = artifact_dir / "web_test"

    # Start fresh every run to avoid stale test state
    if web_test_root.exists():
        shutil.rmtree(web_test_root)
    web_test_root.mkdir(parents=True, exist_ok=True)

    CUI.info(f"Preparing web test environment in {web_test_root}...")

    # Copy everything from web/ except node_modules (deps go to the artifact dir)
    for item in web_src.iterdir():
        if item.name == "node_modules":
            continue
        dest = web_test_root / item.name
        if item.is_dir():
            shutil.copytree(item, dest)
        else:
            shutil.copy2(item, dest)

    npm_cmd = ["npm.cmd"] if sys.platform.startswith("win") else ["npm"]

    if not (web_test_root / "node_modules").exists():
        CUI.info("Installing npm dependencies in artifact dir...")
        if not CRunner.run(
            npm_cmd + ["install", "--silent"], cwd=web_test_root, exit_on_fail=False
        ):
            CUI.error("npm install failed.")
            return False

    return CRunner.run(npm_cmd + ["test"], cwd=web_test_root, exit_on_fail=False)


# ---------------------------------------------------------------------------
# Web server helpers
# ---------------------------------------------------------------------------

def _prepare_web_server_env(out_web_dir: Path):
    """
    Copy web source files to out_web_dir (excluding node_modules and tests/),
    then download the fbc EPUB converter into out_web_dir/tools/.
    The web/ source directory is never touched.
    """
    CUI.info(f"Preparing web server environment in {out_web_dir}...")
    out_web_dir.mkdir(parents=True, exist_ok=True)

    web_src = CPaths.REPO_ROOT / "web"

    # tests/ are source-only; node_modules belong in artifacts, not deployed files
    skip = {"node_modules", "tests"}

    for item in web_src.iterdir():
        if item.name in skip:
            continue
        dest = out_web_dir / item.name
        if item.is_dir():
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(item, dest)
        else:
            shutil.copy2(item, dest)

    # Download the EPUB converter (skipped if already present)
    CRunner.DownloadFbc(out_web_dir / "tools")


def _run_npm_install(work_dir: Path) -> bool:
    """
    Install npm dependencies in work_dir.
    Always runs 'npm install' since source files (including package.json) may have
    been refreshed by _prepare_web_server_env. npm install is idempotent and fast
    when dependencies are already up to date.
    """
    CUI.info("Installing/verifying npm dependencies in artifacts dir...")
    npm_cmd = ["npm.cmd"] if sys.platform.startswith("win") else ["npm"]
    if not CRunner.run(
        npm_cmd + ["install", "--silent"], cwd=work_dir, exit_on_fail=False
    ):
        CUI.error("npm install failed.")
        return False
    return True


def _generate_web_configs(preset: str, library_path: str, out_web_dir: Path) -> bool:
    """
    Write librium_config.json and web_config.json to out_web_dir.
    Detects the .inpx file and archives directory automatically.
    All paths are stored with forward slashes for cross-platform JSON.
    """
    CUI.info("Generating configuration files...")
    out_web_dir.mkdir(parents=True, exist_ok=True)

    lib_path = Path(library_path)
    if not lib_path.exists():
        CUI.error(f"Library path does not exist: {lib_path}")
        return False

    inpx_files = list(lib_path.glob("*.inpx"))
    if not inpx_files:
        CUI.error(f"No .inpx file found in: {lib_path}")
        return False
    inpx_path = inpx_files[0]

    # Detect archives directory: prefer 'lib.rus.ec' subfolder, then any dir with ZIPs
    archives_dir = lib_path / "lib.rus.ec"
    if not archives_dir.exists():
        for sub in lib_path.iterdir():
            if sub.is_dir() and any(sub.glob("*.zip")):
                archives_dir = sub
                break
    if not archives_dir.exists():
        archives_dir = lib_path

    db_path = out_web_dir / "library.db"
    log_path = out_web_dir / "librium.log"

    def _fwd(p: Path) -> str:
        return str(p.resolve()).replace("\\", "/")

    librium_config = {
        "database": {"path": _fwd(db_path)},
        "library": {
            "inpxPath": _fwd(inpx_path),
            "archivesDir": _fwd(archives_dir),
        },
        "import": {
            "parseFb2": True,
            "threadCount": 8,
            "transactionBatchSize": 1000,
            "mode": "full",
        },
        "filters": {
            "excludeLanguages": [],
            "includeLanguages": [],
            "excludeGenres": [],
            "includeGenres": [],
            "minFileSize": 0,
            "maxFileSize": 0,
            "excludeAuthors": [],
            "excludeKeywords": [],
        },
        "logging": {
            "level": "info",
            "file": _fwd(log_path),
            "progressInterval": 1000,
        },
    }

    librium_cfg_path = out_web_dir / "librium_config.json"
    with open(librium_cfg_path, "w", encoding="utf-8") as fh:
        json.dump(librium_config, fh, indent=4)

    build_dir = CPaths.get_build_dir(preset)
    binary_path = CPaths.get_executable_path(build_dir, "apps/Librium", "Librium")
    if not binary_path.exists():
        CUI.error(f"Librium binary not found: {binary_path}")
        return False

    web_config = {
        "libriumExe": _fwd(binary_path),
        "libriumConfig": _fwd(librium_cfg_path),
        "libriumPort": 9001,
        "webPort": 8080,
        "tempDir": _fwd(out_web_dir / "temp"),
        "metaDir": _fwd(out_web_dir / "meta"),
        "toolsDir": _fwd(out_web_dir / "tools"),
        "fb2cngExe": "",
    }

    web_cfg_path = out_web_dir / "web_config.json"
    with open(web_cfg_path, "w", encoding="utf-8") as fh:
        json.dump(web_config, fh, indent=4)

    return True


def _generate_demo_library(out_dir: Path) -> Path:
    """
    Generate a synthetic demo library of ~350 books with covers.
    Reuses the existing one if the output directory is already populated.
    Returns the path to the test_library subfolder.
    """
    demo_lib_dir = out_dir / "demo_lib"
    lib_path = demo_lib_dir / "test_library"

    if lib_path.exists() and any(lib_path.glob("*.inpx")):
        CUI.info(f"Reusing existing demo library at: {lib_path}")
        return lib_path

    CUI.info("Generating synthetic demo library (~350 books, with covers)...")
    if not CRunner.run_python(
        "LibraryGenerator.py",
        ["--out", str(demo_lib_dir), "--books", "350", "--archive-size", "30", "--covers"],
        exit_on_fail=False,
    ):
        CUI.error("Failed to generate demo library.")
        sys.exit(1)

    return lib_path


def _do_smoke_test(preset: str, library_path: str, folder_name: str) -> bool:
    """
    Run CSmokeTester without rebuilding. Called from both run_smoke_test()
    and run_ci_pipeline() (for the synthetic smoke stage).
    """
    from SmokeTester import CSmokeTester

    build_dir = CPaths.get_build_dir(preset)
    librium_exe = CPaths.get_executable_path(build_dir, "apps/Librium", "Librium")
    output_root = CPaths.get_artifact_dir(preset) / folder_name

    test = CSmokeTester(librium_exe, output_root, library_path)
    return test.run()



def run_ci_pipeline(preset: str, clean: bool, skip_web_tests: bool = False) -> bool:
    """
    Scenario 1: Build + Unit tests + Scenario tests + Web API tests + Synthetic smoke test.
    On Docker (skip_web_tests=True), web API tests are skipped but the synthetic smoke test
    still runs (it only requires the C++ engine).
    """
    CUI.banner(f"LIBRIUM CI PIPELINE [Preset: {preset}]")

    if not _do_build(preset, clean):
        return False

    CUI.banner(f"TESTING LIBRIUM [Preset: {preset}]")

    if not _do_unit_tests(preset):
        return False

    if not _do_scenario_tests(preset):
        return False

    step = 3
    if skip_web_tests:
        CUI.info("Web API tests skipped (not supported in Docker mode).")
    else:
        if not _do_web_tests(preset):
            return False
        step = 4

    # Synthetic smoke test: validates the full import pipeline end-to-end.
    # Always runs (including Docker), because it only requires the C++ engine.
    CUI.step(step, "SYNTHETIC SMOKE TEST")
    lib_dir = CPaths.get_artifact_dir(preset) / "synthetic_lib"
    if lib_dir.exists():
        shutil.rmtree(lib_dir)
    if not CRunner.run_python(
        "LibraryGenerator.py",
        ["--out", str(lib_dir), "--books", "100", "--archive-size", "10", "--covers"],
        exit_on_fail=False,
    ):
        CUI.error("Failed to generate synthetic library for smoke test.")
        return False
    if not _do_smoke_test(preset, str(lib_dir / "test_library"), "synthetic_run"):
        return False

    return True


def run_smoke_test(preset: str, clean: bool, library_path: str) -> bool:
    """Scenario 2: Build + Smoke test on a real (or custom) library."""
    CUI.banner(f"LIBRIUM SMOKE TEST [Preset: {preset}]")

    if not _do_build(preset, clean):
        return False

    CUI.step(1, "SMOKE TEST")

    is_synthetic = "synthetic_lib" in str(library_path) or "demo_lib" in str(library_path)
    folder_name = "synthetic_run" if is_synthetic else "real_run"
    return _do_smoke_test(preset, library_path, folder_name)


def run_web_server(preset: str, clean: bool, demo: bool, library_path: str = None) -> bool:
    """
    Scenarios 3 & 4: Build + Start web server.
    In demo mode, generates (or reuses) a synthetic library automatically.
    All web artifacts are isolated under out/artifacts/web/.
    """
    CUI.banner(f"LIBRIUM WEB SERVER [Preset: {preset}]")

    if not _do_build(preset, clean):
        return False

    out_web_dir = CPaths.ARTIFACTS_ROOT / "web"
    step = 1

    if demo:
        CUI.step(step, "GENERATING DEMO LIBRARY")
        step += 1
        library_path = str(_generate_demo_library(out_web_dir))

    CUI.step(step, "PREPARING WEB ENVIRONMENT")
    step += 1
    _prepare_web_server_env(out_web_dir)

    CUI.step(step, "GENERATING CONFIGS")
    step += 1
    if not _generate_web_configs(preset, library_path, out_web_dir):
        return False

    CUI.step(step, "NPM INSTALL")
    if not _run_npm_install(out_web_dir):
        return False

    CUI.banner("STARTING WEB SERVER")
    CUI.info("URL: http://localhost:8080")
    CUI.info("Press Ctrl+C to stop.")
    try:
        result = subprocess.run(["node", "server.js"], cwd=str(out_web_dir))
        if result.returncode != 0:
            CUI.error(f"Web server exited with code {result.returncode}.")
            return False
    except KeyboardInterrupt:
        CUI.info("Web server stopped by user.")

    return True


def run_gen(extra_args: list) -> bool:
    """Advanced: generate a synthetic library by forwarding args to LibraryGenerator.py."""
    return CRunner.run_python("LibraryGenerator.py", extra_args, exit_on_fail=False)


def run_docker(preset: str, clean: bool) -> bool:
    """
    Route the CI pipeline into a Linux Docker container.
    Only the CI pipeline is supported in Docker; web server and smoke tests are not.
    """
    CUI.banner(f"LIBRIUM DOCKER PIPELINE [Preset: {preset}]")

    image_name = "librium-build-linux"
    dockerfile = "Dockerfile.linux"

    CUI.info("PHASE 1: ENSURING DOCKER IMAGE IS READY")
    if not CRunner.run(
        ["docker", "build", "-t", image_name, "-f", str(CPaths.REPO_ROOT / dockerfile), "."],
        exit_on_fail=False,
    ):
        CUI.error("Docker image build failed.")
        return False

    CUI.info("PHASE 2: EXECUTING CI PIPELINE INSIDE LINUX CONTAINER")
    inner_cmd = f"python3 -u scripts/Run.py --preset {preset}"
    if clean:
        inner_cmd += " --clean"

    docker_run = [
        "docker", "run", "--rm", "-t",
        "-e", "PYTHONUNBUFFERED=1",
        "-e", "LIBRIUM_IN_DOCKER=1",
        "-v", f"{CPaths.REPO_ROOT}:/app",
        image_name, "bash", "-c", inner_cmd,
    ]
    return CRunner.run(docker_run, exit_on_fail=False)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def _parse_args():
    parser = argparse.ArgumentParser(
        prog="Run.py",
        description="Librium unified automation pipeline — build, test, and serve.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=_HELP_EPILOG,
    )

    parser.add_argument(
        "--preset",
        default="x64-debug",
        choices=["x64-debug", "x64-release", "linux-debug", "linux-release"],
        help="CMake preset to use (default: x64-debug)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build and artifact directories before building",
    )
    parser.add_argument(
        "--library",
        metavar="PATH",
        help=(
            "Path to a library folder (must contain a .inpx file and archive ZIPs). "
            "Without --web: runs a smoke test. "
            "With --web: starts the web server using this library."
        ),
    )
    parser.add_argument(
        "--web",
        action="store_true",
        help=(
            "Start the web server instead of running tests. "
            "Requires --demo or --library."
        ),
    )
    parser.add_argument(
        "--demo",
        action="store_true",
        help=(
            "Generate and use a synthetic demo library (~350 books with covers). "
            "Only valid with --web. The library is reused on subsequent runs."
        ),
    )

    args = parser.parse_args()

    # Validate mutual dependencies
    if args.demo and not args.web:
        parser.error("--demo requires --web")
    if args.web and not args.demo and not args.library:
        parser.error("--web requires either --demo or --library PATH")

    return args


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    # 'gen' is a special subcommand that accepts arbitrary extra arguments,
    # so we handle it before full argparse to avoid interference.
    # It must be the first positional argument: python Run.py gen [...]
    if len(sys.argv) > 1 and sys.argv[1] == "gen":
        CUI.banner("GENERATING SYNTHETIC LIBRARY")
        success = run_gen(sys.argv[2:])
        CUI.banner("COMPLETED SUCCESSFULLY" if success else "FAILED")
        if not success:
            sys.exit(1)
        return

    if "gen" in sys.argv[2:]:
        CUI.error(
            "'gen' must be the first argument.\n"
            "  Correct usage: python Run.py gen [--out PATH] [--books N] ...\n"
            "  Got:           python Run.py " + " ".join(sys.argv[1:])
        )
        sys.exit(1)
    args = _parse_args()

    is_linux_preset = args.preset.startswith("linux")
    in_docker = bool(os.environ.get("LIBRIUM_IN_DOCKER"))

    if is_linux_preset and not in_docker:
        # Route to Docker; only CI pipeline is supported there
        if args.web:
            CUI.error(
                "Web server mode (--web) is not supported in Docker. "
                "Use a native preset (x64-debug or x64-release)."
            )
            sys.exit(1)
        if args.library:
            CUI.error(
                "Smoke test (--library) is not supported in Docker. "
                "Use a native preset (x64-debug or x64-release)."
            )
            sys.exit(1)
        success = run_docker(args.preset, args.clean)

    elif in_docker:
        # Running inside the Docker container itself
        if args.web:
            CUI.error("Web server mode is not supported inside Docker.")
            sys.exit(1)
        if args.library:
            CUI.error("Smoke test (--library) is not supported inside Docker.")
            sys.exit(1)
        # Web tests are skipped in Docker (no npm/Node.js in the build image)
        success = run_ci_pipeline(args.preset, args.clean, skip_web_tests=True)

    elif args.library and not args.web:
        # Smoke test on a real (or custom) library
        success = run_smoke_test(args.preset, args.clean, args.library)

    elif args.web:
        # Web server (demo or real library)
        success = run_web_server(args.preset, args.clean, args.demo, args.library)

    else:
        # Default: full CI pipeline
        success = run_ci_pipeline(args.preset, args.clean, skip_web_tests=False)

    CUI.banner("COMPLETED SUCCESSFULLY" if success else "FAILED")
    if not success:
        sys.exit(1)


if __name__ == "__main__":
    main()
