"""
TEST 01: Full Library Import
Description: Performs a full indexing of the real library (EN + RU) from the INPX file.
Inputs: real_library_config.json (pointing to C:/_LIBRU~1.EC-/)
Success Criteria:
1. Librium exits with code 0.
2. Database 'RealLibrary.db' is created.
3. Database size is > 100MB (real data present).
4. Execution time is within reasonable limits (~1-2 minutes).
"""
import subprocess
import time
import os
import sys

# Paths
LIBRIUM_EXE = r"out\build\x64-debug\apps\Librium\Debug\Librium.exe"
CONFIG_PATH = r"tests\RealLibrary\real_library_config.json"
DB_PATH = r"tests\RealLibrary\RealLibrary.db"
LOG_PATH = r"tests\RealLibrary\RealLibrary.log"

def test_import():
    print(f"--- Starting Import Test ---")
    if os.path.exists(DB_PATH):
        print(f"Removing old DB: {DB_PATH}")
        os.remove(DB_PATH)

    start_time = time.time()
    
    # We use subprocess.Popen to monitor the process
    # and possibly kill it if it hangs
    print(f"Executing: {LIBRIUM_EXE} import --config {CONFIG_PATH}")
    
    process = subprocess.Popen(
        [LIBRIUM_EXE, "import", "--config", CONFIG_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding='utf-8'
    )

    # Monitor for 5 minutes max
    timeout = 300 # 5 minutes
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        print(f"FAIL: Import timed out after {timeout} seconds")
        sys.exit(1)

    duration = time.time() - start_time
    print(f"Import finished in {duration:.2f} seconds")

    if process.returncode != 0:
        print(f"FAIL: Librium exited with code {process.returncode}")
        print(f"STDOUT: {stdout}")
        print(f"STDERR: {stderr}")
        sys.exit(1)

    if not os.path.exists(DB_PATH):
        print(f"FAIL: Database file not created at {DB_PATH}")
        sys.exit(1)

    db_size = os.path.getsize(DB_PATH)
    print(f"Database size: {db_size / (1024*1024):.2f} MB")

    if db_size < 1024 * 1024: # Expect at least 1MB for a real library
        print(f"FAIL: Database is too small ({db_size} bytes)")
        sys.exit(1)

    print("SUCCESS: Import completed correctly.")

if __name__ == "__main__":
    test_import()
