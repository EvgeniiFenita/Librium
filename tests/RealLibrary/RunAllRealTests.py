import subprocess
import os
import shutil
import sys

TESTS_DIR = "tests/RealLibrary"
SCRIPTS = [
    "01_TestImport.py",
    "02_SearchQuality.py",
    "03_CheckLogs.py",
    "04_TestFiltering.py",
    "05_TestExport.py",
    "06_TestFb2Parsing.py"
]

def cleanup():
    print("\n--- Cleaning up artifacts ---")
    extensions_to_remove = [".db", ".log"]
    files_to_remove = ["query_result.json", "export_query.json", "fb2_query.json"]
    dirs_to_remove = ["exported"]

    for file in os.listdir(TESTS_DIR):
        # Remove by extension
        if any(file.endswith(ext) for ext in extensions_to_remove):
            os.remove(os.path.join(TESTS_DIR, file))
        # Remove specific output JSONs
        if file in files_to_remove:
            os.remove(os.path.join(TESTS_DIR, file))
            
    for d in dirs_to_remove:
        path = os.path.join(TESTS_DIR, d)
        if os.path.exists(path):
            shutil.rmtree(path)
    print("Cleanup complete.")

def run_all():
    print("=== Running Full Real Library Test Suite ===")
    for script in SCRIPTS:
        print(f"\n>> Running {script}...")
        # Use sys.executable to ensure we use the same python interpreter
        res = subprocess.run([sys.executable, os.path.join(TESTS_DIR, script)])
        if res.returncode != 0:
            print(f"\n[!] TEST SUITE FAILED at {script}")
            print("[!] Artifacts preserved for diagnostics.")
            sys.exit(1)
    
    print("\n=== ALL TESTS PASSED SUCCESSFULLY ===")
    cleanup()

if __name__ == "__main__":
    run_all()
