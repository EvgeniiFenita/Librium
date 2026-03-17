"""
TEST 04: Import Filtering Logic
Description: Tests the 'includeLanguages' filter by creating a specialized database 
             containing ONLY English books.
Inputs: filter_library_config.json
Success Criteria:
1. Database 'Filtered.db' contains exactly the expected number of English books (~37k).
2. No Russian books ('ru') are present in the 'stats' output.
"""
import subprocess
import os
import sys

LIBRIUM_EXE = os.environ.get("LIBRIUM_EXE", os.path.join("out", "build", "x64-debug", "apps", "Librium", "Debug", "Librium.exe"))
artifact_dir = os.environ.get("LIBRIUM_ARTIFACT_DIR", os.path.join("tests", "RealLibrary"))
config_name = "filter_library_config.json"
CONFIG_PATH = os.path.join(artifact_dir, config_name)
if not os.path.exists(CONFIG_PATH):
    CONFIG_PATH = os.path.join("tests", "RealLibrary", config_name)
DB_PATH = os.path.join(artifact_dir, "Filtered.db")

def test_filtering():
    print("--- Starting Filtering Import Test (Language: 'en' only) ---")
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)

    # 1. Import with filter
    print(f"Executing import with 'en' filter...")
    subprocess.run([LIBRIUM_EXE, "import", "--config", CONFIG_PATH], check=True)

    # 2. Check stats
    result = subprocess.run([LIBRIUM_EXE, "stats", "--db", DB_PATH], capture_output=True, text=True, encoding='utf-8')
    output = result.stdout
    print(f"Stats output:\n{output.strip()}")

    # We expect ~37,927 books in English (based on previous run)
    # Let's check for 'en :' and ensure no 'ru :'
    if "en :" in output and "37927" in output:
        print("SUCCESS: Found correct number of English books.")
    else:
        print("FAIL: Book count mismatch for English books.")
        sys.exit(1)

    if "ru :" in output:
        print("FAIL: Found Russian books despite filter.")
        sys.exit(1)

    print("SUCCESS: Filtering Import Test passed.")

if __name__ == "__main__":
    test_filtering()
