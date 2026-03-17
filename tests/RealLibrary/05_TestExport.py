"""
TEST 05: Real Archive Export
Description: Tests the end-to-end flow of finding a book in the database and 
             extracting the actual FB2 file from its corresponding ZIP archive.
Inputs: RealLibrary.db, ZIP archives in lib.rus.ec/
Success Criteria:
1. Find a book in the database whose ZIP archive actually exists on disk.
2. 'export' command extracts the file successfully.
3. Extracted file is a valid XML/FB2 (checked by peeking at header).
"""
import subprocess
import os
import sys
import json
import shutil

LIBRIUM_EXE = os.environ.get("LIBRIUM_EXE", os.path.join("out", "build", "x64-debug", "apps", "Librium", "Debug", "Librium.exe"))
artifact_dir = os.environ.get("LIBRIUM_ARTIFACT_DIR", os.path.join("tests", "RealLibrary"))
config_name = "real_library_config.json"
CONFIG_PATH = os.path.join(artifact_dir, config_name)
if not os.path.exists(CONFIG_PATH):
    CONFIG_PATH = os.path.join("tests", "RealLibrary", config_name)
DB_PATH = os.path.join(artifact_dir, "RealLibrary.db")
QUERY_OUT = os.path.join(artifact_dir, "export_query.json")
EXPORT_DIR = os.path.join(artifact_dir, "exported")

def test_export():
    print("--- Starting Export Test (Robust) ---")
    if os.path.exists(EXPORT_DIR):
        shutil.rmtree(EXPORT_DIR)
    os.makedirs(EXPORT_DIR)

    # 0. Get library archives dir from config to check for existence
    with open(CONFIG_PATH, 'r', encoding='utf-8') as f:
        cfg = json.load(f)
        archives_dir = cfg["library"]["archivesDir"]

    # 1. Find some books (more books, from the end too)
    print("Searching for books to export...")
    # Offset by 1000 to avoid possibly missing first archives
    cmd = [LIBRIUM_EXE, "query", "--db", DB_PATH, "--output", QUERY_OUT, "--limit", "500", "--offset", "1000"]
    subprocess.run(cmd, check=True)

    with open(QUERY_OUT, 'r', encoding='utf-8') as f:
        data = json.load(f)
        if not data["books"]:
            print("FAIL: Could not find any books to test export.")
            sys.exit(1)
        
        target_book = None
        for b in data["books"]:
            archive_base = b.get("archiveName") # Note: was 'archive' in my previous edit, should be 'archiveName'
            if not archive_base: continue
            
            # Check multiple possible filenames
            found = False
            for ext in ["", ".zip", ".ZIP"]:
                archive_path = os.path.join(archives_dir, archive_base + ext)
                if os.path.exists(archive_path):
                    target_book = b
                    print(f"Found valid book for export: ID={b['id']}, Archive={archive_base + ext}")
                    found = True
                    break
            if found: break
        
        if not target_book:
            print(f"FAIL: Could not find any book in the records whose archive exists in {archives_dir}")
            print(f"Sample archive we looked for: {data['books'][0].get('archiveName')}.zip")
            sys.exit(1)

        book_id = target_book["id"]

    # 2. Export book
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
    
    if size < 500: # Some FB2 files are small, but let's expect at least something
        print("FAIL: Exported file is too small.")
        sys.exit(1)

    with open(exported_file, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read(512)
        print(f"Peeking into file: {content[:100].strip()}...")
        # Check for common XML/FB2 markers
        content_lower = content.lower()
        if "<?xml" in content_lower or "<fictionbook" in content_lower or "<binary" in content_lower:
            print("SUCCESS: File looks like a valid FB2/XML.")
        else:
            print("FAIL: File does not look like FB2 (header check failed).")
            sys.exit(1)

if __name__ == "__main__":
    test_export()
