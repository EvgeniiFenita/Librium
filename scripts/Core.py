import os
import sys
import subprocess
from pathlib import Path

# Prevent creation of __pycache__ folders in source directories
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

# Ensure UTF-8 output on Windows (CP1252 terminal cannot encode Cyrillic/Greek etc.)
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

class CPaths:
    REPO_ROOT = Path(__file__).parent.parent.resolve()
    SCRIPTS_DIR = REPO_ROOT / "scripts"
    OUT_DIR = REPO_ROOT / "out"
    BUILD_ROOT = OUT_DIR / "build"
    ARTIFACTS_ROOT = OUT_DIR / "artifacts"
    TESTS_DIR = REPO_ROOT / "tests"
    SCENARIOS_DIR = TESTS_DIR / "Scenarios"

    @staticmethod
    def get_build_dir(preset: str) -> Path:
        return CPaths.BUILD_ROOT / preset
        
    @staticmethod
    def get_artifact_dir(preset: str) -> Path:
        return CPaths.ARTIFACTS_ROOT / preset

    @staticmethod
    def get_config(preset: str) -> str:
        return "Release" if "release" in preset.lower() else "Debug"

    @staticmethod
    def get_executable_path(build_dir: Path, subpath: str, name: str) -> Path:
        is_win = sys.platform.startswith("win")
        config = "Release" if "release" in str(build_dir).lower() else "Debug"
        ext = ".exe" if is_win else ""
        
        # On Windows, CMake usually puts executables in build/path/Config/name.exe
        # On Linux, it's usually build/path/name
        if is_win:
            exe_path = build_dir / subpath / config / f"{name}{ext}"
        else:
            exe_path = build_dir / subpath / f"{name}{ext}"
            
        return exe_path

class CUI:
    @staticmethod
    def banner(title: str):
        print("\n" + "="*80)
        print(f"  {title}")
        print("="*80, flush=True)

    @staticmethod
    def step(step_no: int, title: str):
        print(f"\n--- [STEP {step_no}] {title} ---", flush=True)

    @staticmethod
    def info(msg: str):
        print(f">>> {msg}", flush=True)

    @staticmethod
    def warn(msg: str):
        print(f"[!] WARN: {msg}", flush=True, file=sys.stderr)

    @staticmethod
    def error(msg: str):
        print(f"\n[!] ERROR: {msg}", flush=True, file=sys.stderr)

class CRunner:
    @staticmethod
    def run(cmd: list, cwd: Path = None, exit_on_fail: bool = True) -> bool:
        """Runs a command and returns True if successful."""
        try:
            res = subprocess.run(cmd, cwd=cwd or CPaths.REPO_ROOT)
            if res.returncode != 0:
                if exit_on_fail:
                    CUI.error(f"Command failed with exit code {res.returncode}: {' '.join(map(str, cmd))}")
                    sys.exit(res.returncode)
                return False
            return True
        except Exception as e:
            CUI.error(f"Failed to execute command: {e}")
            if exit_on_fail:
                sys.exit(1)
            return False

    @staticmethod
    def run_python(script_name: str, args: list = None, exit_on_fail: bool = True) -> bool:
        cmd = [sys.executable, "-u", str(CPaths.SCRIPTS_DIR / script_name)]
        if args:
            cmd.extend(args)
        return CRunner.run(cmd, exit_on_fail=exit_on_fail)

    @staticmethod
    def DownloadFbc(tools_dir: Path):
        """Download and extract fbc (fb2cng) converter if not present."""
        import platform
        import urllib.request
        import json
        import zipfile
        
        system = platform.system().lower()
        arch = platform.machine().lower()
        
        exe_name = "fbc.exe" if system == "windows" else "fbc"
        if (tools_dir / exe_name).exists():
            return

        CUI.info(f"Downloading EPUB converter (fbc) for {system} {arch}...")
        tools_dir.mkdir(parents=True, exist_ok=True)

        asset_pattern = ""
        if system == "windows":
            if "amd64" in arch or "x86_64" in arch: asset_pattern = "windows-amd64.zip"
            elif "arm64" in arch: asset_pattern = "windows-arm64.zip"
            else: asset_pattern = "windows-386.zip"
        elif system == "linux":
            if "amd64" in arch or "x86_64" in arch: asset_pattern = "linux-amd64.zip"
            elif "arm64" in arch: asset_pattern = "linux-arm64.zip"
            else: asset_pattern = "linux-386.zip"
        elif system == "darwin":
            if "arm64" in arch: asset_pattern = "darwin-arm64.zip"
            else: asset_pattern = "darwin-amd64.zip"

        if not asset_pattern:
            CUI.error(f"Unsupported platform for fbc: {system} {arch}. EPUB conversion will be disabled.")
            return

        try:
            release_url = "https://api.github.com/repos/rupor-github/fb2cng/releases/latest"
            with urllib.request.urlopen(urllib.request.Request(release_url, headers={"User-Agent": "Librium-Runner"})) as response:
                release_data = json.loads(response.read().decode())
            
            asset = next((a for a in release_data["assets"] if a["name"].startswith("fbc-") and a["name"].endswith(asset_pattern)), None)
            if not asset:
                CUI.error(f"Could not find fbc asset matching {asset_pattern}")
                return

            archive_path = tools_dir / asset["name"]
            CUI.info(f"Downloading {asset['browser_download_url']}...")
            urllib.request.urlretrieve(asset["browser_download_url"], archive_path)
            
            CUI.info(f"Extracting {archive_path.name}...")
            resolved_tools_dir = tools_dir.resolve()
            with zipfile.ZipFile(archive_path, 'r') as zip_ref:
                for member in zip_ref.infolist():
                    target_path = (tools_dir / member.filename).resolve()
                    if not target_path.is_relative_to(resolved_tools_dir):
                        raise Exception(f"Zip slip attempt detected: {member.filename}")
                    zip_ref.extract(member, tools_dir)
            
            archive_path.unlink()
            
            if system != "windows":
                (tools_dir / exe_name).chmod(0o755)
                
            CUI.info("EPUB converter installed successfully.")
        except Exception as e:
            CUI.warn(f"Failed to download EPUB converter: {e}")
