"""
TEST 05: Real Archive Export
Description: Tests the end-to-end flow of finding a book in the database and 
             extracting the actual FB2 file from its corresponding ZIP archive.
Inputs: RealLibrary.db, ZIP archives in lib.rus.ec/
Success Criteria:
1. Correct book ID is found via query.
2. 'export' command extracts the file successfully.
3. Extracted file is a valid XML/FB2 (checked by peeking at header).
"""
import subprocess
import os
import sys
import json
import shutil

LIBRIUM_EXE = r"out\build\x64-debug\apps\Librium\Debug\Librium.exe"
DB_PATH = r"tests\RealLibrary\RealLibrary.db"
# Use the config we already have, it has the correct UTF-8 paths
CONFIG_PATH = r"tests\RealLibrary\real_library_config.json"
QUERY_OUT = r"tests\RealLibrary\export_query.json"
EXPORT_DIR = r"tests\RealLibrary\exported"

def test_export():
    print("--- Starting Export Test (Via Config) ---")
    if os.path.exists(EXPORT_DIR):
        shutil.rmtree(EXPORT_DIR)
    os.makedirs(EXPORT_DIR)

    # 1. Find book ID
    print("Finding book ID...")
    cmd = [LIBRIUM_EXE, "query", "--db", DB_PATH, "--output", QUERY_OUT, "--title", "Евангелие от Афрания"]
    subprocess.run(cmd, check=True)

    with open(QUERY_OUT, 'r', encoding='utf-8') as f:
        data = json.load(f)
        if not data["books"]:
            print("FAIL: Could not find book ID.")
            sys.exit(1)
        book_id = data["books"][0]["id"]
        print(f"Found book ID: {book_id}")

    # 2. Export book using --config to avoid CLI encoding issues with paths
    print(f"Exporting book ID {book_id} to {EXPORT_DIR}...")
    cmd = [LIBRIUM_EXE, "export", "--config", CONFIG_PATH, "--id", str(book_id), "--out", EXPORT_DIR]
    
    result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
    if result.returncode != 0:
        print(f"FAIL: Export failed with code {result.returncode}")
        print(f"STDOUT: {result.stdout}")
        print(f"STDERR: {result.stderr}")
        sys.exit(1)

    # 3. Verify file
    files = os.listdir(EXPORT_DIR)
    if not files:
        print(f"FAIL: No files found in {EXPORT_DIR}")
        sys.exit(1)

    exported_file = os.path.join(EXPORT_DIR, files[0])
    size = os.path.getsize(exported_file)
    print(f"Exported file: {exported_file}, size: {size} bytes")
    
    if size < 1000:
        print("FAIL: Exported file is too small.")
        sys.exit(1)

    with open(exported_file, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read(200)
        print(f"Peeking into file: {content[:100].strip()}...")
        if "<?xml" in content.lower() or "<fictionbook" in content.lower():
            print("SUCCESS: File looks like a valid FB2/XML.")
        else:
            print("FAIL: File does not look like FB2.")
            sys.exit(1)

if __name__ == "__main__":
    test_export()
