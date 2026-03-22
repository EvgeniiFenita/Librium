#!/usr/bin/env python3
import sys
import os
import argparse
import subprocess
import json
import shutil
from pathlib import Path

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

# Add scripts dir to path so we can import Core
sys.path.append(str(Path(__file__).parent))
from Core import CPaths, CUI, CRunner

def run_npm_install(work_dir: Path):
    node_modules = work_dir / "node_modules"
    if not node_modules.exists():
        CUI.info("Installing npm dependencies in artifacts dir...")
        subprocess.run(["npm", "install", "--silent"], cwd=str(work_dir), check=True, shell=sys.platform.startswith('win'))
    else:
        CUI.info("npm dependencies already installed.")

def prepare_web_env(src_web_dir: Path, out_web_dir: Path):
    CUI.info(f"Preparing web environment in {out_web_dir}...")
    out_web_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy source files to artifacts dir
    files_to_copy = ["server.js", "package.json"]
    for f in files_to_copy:
        shutil.copy2(src_web_dir / f, out_web_dir / f)
        
    # Copy public folder
    dest_public = out_web_dir / "public"
    if dest_public.exists():
        shutil.rmtree(dest_public)
    shutil.copytree(src_web_dir / "public", dest_public)

def generate_configs(args, out_dir: Path):
    CUI.info("Generating configs...")
    out_dir.mkdir(parents=True, exist_ok=True)
    
    # ... (rest of path detection same)
    lib_path = Path(args.library)
    if not lib_path.exists():
        CUI.error(f"Library path {lib_path} does not exist.")
        sys.exit(1)

    inpx_files = list(lib_path.glob("*.inpx"))
    if not inpx_files:
        CUI.error(f"No .inpx file found in {lib_path}")
        sys.exit(1)
    inpx_path = inpx_files[0]

    archives_dir = lib_path / "lib.rus.ec"
    if not archives_dir.exists():
        for sub in lib_path.iterdir():
            if sub.is_dir() and any(sub.glob("*.zip")):
                archives_dir = sub
                break
    
    if not archives_dir.exists():
        archives_dir = lib_path

    db_path = out_dir / "library.db"
    log_path = out_dir / "librium.log"
    
    librium_config = {
        "database": { "path": str(db_path.resolve()).replace('\\', '/') },
        "library": {
            "inpxPath": str(inpx_path.resolve()).replace('\\', '/'),
            "archivesDir": str(archives_dir.resolve()).replace('\\', '/')
        },
        "import": {
            "parseFb2": True,
            "threadCount": 8,
            "transactionBatchSize": 1000,
            "mode": "full"
        },
        "filters": {
            "excludeLanguages": [],
            "includeLanguages": [],
            "excludeGenres": [],
            "includeGenres": [],
            "minFileSize": 0,
            "maxFileSize": 0,
            "excludeAuthors": [],
            "excludeKeywords": []
        },
        "logging": {
            "level": "info",
            "file": str(log_path.resolve()).replace('\\', '/'),
            "progressInterval": 1000
        }
    }
    
    librium_cfg_path = out_dir / "librium_config.json"
    with open(librium_cfg_path, 'w', encoding='utf-8') as f:
        json.dump(librium_config, f, indent=4)
        
    binary_path = CPaths.get_executable_path(CPaths.get_build_dir(args.preset), "apps/Librium", "Librium")
    if not binary_path.exists():
        CUI.error(f"Binary not found at {binary_path}")
        sys.exit(1)
        
    web_config = {
        "libriumExe": str(binary_path.resolve()).replace('\\', '/'),
        "libriumConfig": str(librium_cfg_path.resolve()).replace('\\', '/'),
        "libriumPort": 9001,
        "webPort": 8080,
        "tempDir": str((out_dir / "temp").resolve()).replace('\\', '/'),
        "metaDir": str((out_dir / "meta").resolve()).replace('\\', '/')
    }
    
    web_cfg_path = out_dir / "web_config.json"
    with open(web_cfg_path, 'w', encoding='utf-8') as f:
        json.dump(web_config, f, indent=4)
        
    return web_cfg_path

def main():
    parser = argparse.ArgumentParser(description="Librium Web UI Runner")
    parser.add_argument("--preset", default="x64-debug", help="CMake preset to use")
    parser.add_argument("--library", required=True, help="Path to real library (folder containing lib.inpx and archives/)")
    args = parser.parse_args()

    CUI.banner(f"LIBRIUM WEB UI RUNNER [{args.preset}]")

    # 1. Build C++ Engine
    if not CRunner.run_python("Run.py", ["build", "--preset", args.preset]):
        CUI.error("Build failed.")
        sys.exit(1)

    src_web_dir = CPaths.REPO_ROOT / "web"
    out_web_dir = CPaths.ARTIFACTS_ROOT / "web"
    
    # 2. Prepare environment in out/
    prepare_web_env(src_web_dir, out_web_dir)
    
    # 3. Setup configs in out/
    generate_configs(args, out_web_dir)
    
    # 4. NPM Install in out/
    run_npm_install(out_web_dir)
    
    # 5. Run Web Server from out/
    CUI.info("Starting Web Server...")
    CUI.info("URL: http://localhost:8080")
    
    try:
        subprocess.run(["node", "server.js"], cwd=str(out_web_dir), check=True)
    except KeyboardInterrupt:
        CUI.info("Web Server stopped.")
        
if __name__ == "__main__":
    main()
