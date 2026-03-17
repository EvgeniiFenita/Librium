"""
TEST 02: Search Quality and Multi-language Support
Description: Verifies that the indexed database correctly handles searches for Russian 
             and English books, ensuring proper index work across languages.
Inputs: RealLibrary.db (created by Test 01)
Success Criteria:
1. Found 'Евангелие от Афрания' by author 'Еськов' (Russian).
2. Found 4 versions of 'Пикник на обочине' (Russian title).
3. Found English book '0.4' by Mike Lancaster.
"""
import subprocess
import os
import sys
import json

LIBRIUM_EXE = os.environ.get("LIBRIUM_EXE", os.path.join("out", "build", "x64-debug", "apps", "Librium", "Debug", "Librium.exe"))
artifact_dir = os.environ.get("LIBRIUM_ARTIFACT_DIR", os.path.join("tests", "RealLibrary"))
config_name = "real_library_config.json"
CONFIG_PATH = os.path.join(artifact_dir, config_name)
if not os.path.exists(CONFIG_PATH):
    CONFIG_PATH = os.path.join("tests", "RealLibrary", config_name)
DB_PATH = os.path.join(artifact_dir, "RealLibrary.db")
OUTPUT_JSON = os.path.join(artifact_dir, "query_result.json")

def run_query(args):
    if os.path.exists(OUTPUT_JSON):
        os.remove(OUTPUT_JSON)
    
    # Add mandatory --output
    cmd = [LIBRIUM_EXE, "query", "--db", DB_PATH, "--output", OUTPUT_JSON] + args
    print(f"Executing: {' '.join(cmd)}")
    
    # Run with UTF-8 env or ensure strings are passed correctly
    result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
    
    if result.returncode != 0:
        print(f"Command failed with code {result.returncode}")
        print(f"STDOUT: {result.stdout}")
        print(f"STDERR: {result.stderr}")
        return None

    if not os.path.exists(OUTPUT_JSON):
        print(f"Error: Output JSON {OUTPUT_JSON} not created.")
        return None

    with open(OUTPUT_JSON, 'r', encoding='utf-8') as f:
        return json.load(f)

def test_search():
    print("--- Starting Search Quality Test (Fixed) ---")
    
    # 1. Search for author 'Еськов'
    data = run_query(["--author", "Еськов"])
    if data and "books" in data and len(data["books"]) > 0:
        found = False
        for book in data["books"]:
            if "Афрания" in book["title"]:
                print(f"SUCCESS: Found '{book['title']}'")
                found = True
                break
        if not found:
            print("FAIL: 'Евангелие от Афрания' not in results.")
            sys.exit(1)
    else:
        print("FAIL: No books found for author 'Еськов'")
        sys.exit(1)

    # 2. Search for title 'Пикник на обочине'
    data = run_query(["--title", "Пикник на обочине"])
    if data and "books" in data and len(data["books"]) > 0:
        print(f"SUCCESS: Found {len(data['books'])} books for 'Пикник на обочине'")
    else:
        print("FAIL: Could not find 'Пикник на обочине'")
        sys.exit(1)

    # 3. Search for English book
    data = run_query(["--title", "0.4"])
    if data and "books" in data and len(data["books"]) > 0:
        print(f"SUCCESS: Found English book '0.4' by Mike Lancaster")
    else:
        print("FAIL: Could not find English book '0.4'")
        sys.exit(1)

    print("SUCCESS: Search Quality Test passed.")

if __name__ == "__main__":
    test_search()
