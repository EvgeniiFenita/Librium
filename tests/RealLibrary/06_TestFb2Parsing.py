"""
TEST 06: Deep FB2 Parsing (Annotations)
Description: Verifies the 'parseFb2' flag functionality by extracting real Russian 
             annotations from files within ZIP archives.
Inputs: fb2_library_config.json (filters for RU + sf_history genre)
Success Criteria:
1. Books are imported with annotations successfully.
2. Exported JSON contains valid Russian text in the 'annotation' field.
3. JSON serialization handles potentially 'dirty' UTF-8 data without crashing.
"""
import subprocess
import os
import sys
import json

LIBRIUM_EXE = r"out\build\x64-debug\apps\Librium\Debug\Librium.exe"
CONFIG_PATH = r"tests\RealLibrary\fb2_library_config.json"
DB_PATH = r"tests\RealLibrary\Fb2Parsed.db"
QUERY_OUT = r"tests\RealLibrary\fb2_query.json"

def test_fb2_parsing():
    print("--- Starting FB2 Deep Parsing Test (Language: 'ru', Genre: 'sf_history') ---")
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)

    # 1. Import with parseFb2=true
    print("Importing with deep FB2 parsing...")
    subprocess.run([LIBRIUM_EXE, "import", "--config", CONFIG_PATH], check=True)

    # 2. Check if annotations were extracted
    print("Verifying annotations in DB...")
    # Use title search to find specific known book or just limit
    cmd = [LIBRIUM_EXE, "query", "--db", DB_PATH, "--output", QUERY_OUT, "--limit", "50"]
    subprocess.run(cmd, check=True)

    with open(QUERY_OUT, 'r', encoding='utf-8') as f:
        data = json.load(f)
        books = data.get("books", [])
        print(f"Checking {len(books)} books for annotations...")
        
        annotated_count = 0
        for book in books:
            ann = book.get("annotation", "")
            if ann and len(ann) > 50:
                annotated_count += 1
                if annotated_count == 1:
                    print(f"Sample annotation (RU) from '{book['title']}':\n{ann[:200]}...")

        if annotated_count > 0:
            print(f"SUCCESS: Found {annotated_count} books with extracted annotations.")
        else:
            print("FAIL: No annotations were extracted from FB2 files.")
            sys.exit(1)

    print("SUCCESS: FB2 Parsing Test passed.")

if __name__ == "__main__":
    test_fb2_parsing()
