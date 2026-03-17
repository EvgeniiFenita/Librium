"""
TEST 03: Log Analysis
Description: Scans the diagnostic logs generated during the full import (Test 01)
             for any [ERROR] or critical [WARN] tags.
Inputs: RealLibrary.log
Success Criteria:
1. Zero [ERROR] messages found in the log file.
"""
import os
import sys

artifact_dir = os.environ.get("LIBRIUM_ARTIFACT_DIR", os.path.join("tests", "RealLibrary"))
LOG_PATH = os.path.join(artifact_dir, "RealLibrary.log")

def check_logs():
    print(f"--- Checking Logs: {LOG_PATH} ---")
    if not os.path.exists(LOG_PATH):
        print(f"FAIL: Log file not found at {LOG_PATH}")
        sys.exit(1)

    errors = 0
    warnings = 0
    with open(LOG_PATH, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            if "[error]" in line.lower():
                print(f"ERROR found: {line.strip()}")
                errors += 1
            if "[warn]" in line.lower():
                # We can decide if warnings are fatal or just notes
                # For now let's just count them
                warnings += 1

    print(f"Summary: {errors} Errors, {warnings} Warnings.")
    
    if errors > 0:
        print("FAIL: Errors found in log.")
        sys.exit(1)
    
    print("SUCCESS: No critical errors found in log.")

if __name__ == "__main__":
    check_logs()
