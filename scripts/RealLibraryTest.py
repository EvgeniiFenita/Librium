#!/usr/bin/env python3
import os
import sys
import json
import time
import base64
import subprocess
import shutil
from pathlib import Path

# Prevent creation of __pycache__ folders in source directories
sys.dont_write_bytecode = True

def print_banner(title):
    print("\n" + "="*80)
    print(f"  {title}")
    print("="*80, flush=True)

def b64_encode(data):
    return base64.b64encode(data.encode('utf-8')).decode('utf-8')

def b64_decode(data):
    return base64.b64decode(data.encode('utf-8')).decode('utf-8')

def send_command(process, action, params):
    cmd = {"action": action, "params": params}
    process.stdin.write(b64_encode(json.dumps(cmd)) + "\n")
    process.stdin.flush()

def read_response(process):
    line = process.stdout.readline().strip()
    if not line:
        return None
    try:
        return json.loads(b64_decode(line))
    except Exception as e:
        print(f"\nProtocol error: {e}. Line: {line}")
        return None

def main():
    if len(sys.argv) < 4:
        print("Usage: RealLibraryTest.py <binary_path> <output_dir> <real_library_path>")
        sys.exit(1)

    binary_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    real_lib_path = Path(sys.argv[3])

    if not real_lib_path.exists():
        print(f"Error: Real library path {real_lib_path} does not exist.")
        sys.exit(1)

    # DETECT ARCHIVES DIRECTORY
    # User said archives are in 'lib.rus.ec' subfolder
    archives_dir = real_lib_path / "lib.rus.ec"
    if not archives_dir.exists():
        # Fallback to root if not found
        archives_dir = real_lib_path
    
    print(f"Detected archives directory: {archives_dir}")

    output_dir.mkdir(parents=True, exist_ok=True)
    db_path = output_dir / "test.db"
    config_path = output_dir / "config.json"
    log_path = output_dir / "librium.log"
    export_dir = output_dir / "exported"

    # FORCE CLEAN START: delete existing DB and ALL related files
    print(f"Cleaning up old test data in {output_dir}...")
    for f in output_dir.glob("test.db*"):
        try: f.unlink()
        except: pass
    if export_dir.exists():
        shutil.rmtree(export_dir)
    covers_dir = output_dir / "meta"
    if covers_dir.exists():
        shutil.rmtree(covers_dir)

    # 1. Setup Config
    config = {
        "database": { "path": str(db_path) },
        "library": { 
            "inpxPath": str(real_lib_path / "librusec_local_fb2.inpx"),
            "archivesDir": str(archives_dir)
        },
        "import": { 
            "threadCount": 32, 
            "parseFb2": True,
            "transactionBatchSize": 2000 
        },
        "logging": { 
            "level": "debug", 
            "file": str(log_path),
            "progressInterval": 1000
        }
    }

    with open(config_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2)

    print_banner("REAL LIBRARY TEST: STARTING ENGINE (CLEAN RUN)")
    process = subprocess.Popen(
        [str(binary_path), "--config", str(config_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding='utf-8',
        errors='ignore',
        bufsize=1
    )

    try:
        # Step 1: Import
        print_banner("STEP 1: IMPORTING LIBRARY (CLEAN)")
        send_command(process, "import", {})
        start_time = time.time()
        last_print_time = 0
        
        while True:
            resp = read_response(process)
            if not resp: break
            status = resp.get("status")
            if status == "progress":
                data = resp.get("data", {})
                processed = data.get("processed", 0)
                total = data.get("total", 0)
                now = time.time()
                elapsed = now - start_time
                speed = processed / elapsed if elapsed > 0 else 0
                pct = (processed * 100 // total) if total > 0 else 0
                if now - last_print_time >= 0.5 or processed == total:
                    print(f"\rProgress: {processed}/{total} ({pct}%) | Speed: {speed:.0f} books/sec", end="", flush=True)
                    last_print_time = now
            elif status == "ok":
                print("\nImport completed successfully.")
                stats = resp.get('data', {})
                print(f"Stats: {json.dumps(stats, indent=2)}")
                if stats.get('fb2_parsed', 0) == 0 and stats.get('inserted', 0) > 0:
                    print("\nERROR: Clean import resulted in 0 FB2 parsed books!")
                    sys.exit(1)
                break
            else:
                print(f"\nImport failed: {resp.get('error')}")
                sys.exit(1)

        # Step 2.2: General Pagination
        print_banner("STEP 2.2: GENERAL PAGINATION")
        total_annotated = 0
        all_retrieved_books = []
        for offset in [0, 50, 100]:
            send_command(process, "query", {"limit": 50, "offset": offset})
            resp = read_response(process)
            if resp and resp.get("status") == "ok":
                books = resp.get("data", {}).get("books", [])
                all_retrieved_books.extend(books)
                annotated = sum(1 for b in books if b.get("annotation"))
                total_annotated += annotated
                print(f"Offset {offset}: Retrieved {len(books)} books. With annotations: {annotated}.")
            else:
                print(f"Query failed: {resp.get('error')}")
                sys.exit(1)

        if all_retrieved_books:
            import random
            print("\n--- DETAILED DATA VERIFICATION (10 Random Samples) ---")
            samples = random.sample(all_retrieved_books, min(10, len(all_retrieved_books)))
            for i, b in enumerate(samples, 1):
                print(f"\n[SAMPLE {i}/10]")
                for key, val in b.items():
                    if key == "annotation" and val:
                        # Show first 200 chars of annotation
                        clean_val = val.replace('\n', ' ')
                        preview = (clean_val[:200] + '...') if len(clean_val) > 200 else clean_val
                        print(f"  {key:15}: {preview}")
                    else:
                        print(f"  {key:15}: {val}")

        print_banner("STEP 2.3: SEARCH AND DETAILS")
        send_command(process, "query", {"author": "Стругацки", "limit": 10})
        resp = read_response(process)
        books_for_export = []
        if resp and resp.get("status") == "ok":
            books = resp.get("data", {}).get("books", [])
            print(f"Found {resp.get('data', {}).get('totalFound')} books by Strugatsky.")
            for b in books[:10]:
                books_for_export.append(b)
                print(f"  - {b.get('title')} (ID: {b.get('id')})")
        else:
            print(f"Search failed: {resp.get('error')}")
            sys.exit(1)

        # Step 3: Export
        print_banner("STEP 3: EXPORTING BOOKS")
        export_dir.mkdir(exist_ok=True)
        export_errors = 0
        for idx, book in enumerate(books_for_export, 1):
            book_id = book.get("id")
            send_command(process, "export", {"id": book_id, "out": str(export_dir)})
            resp = read_response(process)
            if resp and resp.get("status") == "ok":
                file_path = resp.get("data", {}).get("file")
                if not file_path or not os.path.exists(file_path):
                    print(f"[{idx}/10] Export failed: file not found at {file_path}")
                    export_errors += 1
                    continue
                size = os.path.getsize(file_path)
                print(f"[{idx}/10] Exported ID {book_id}: {size / 1024:.1f} KB")
            else:
                print(f"[{idx}/10] Export failed for ID {book_id}: {resp.get('error') if resp else 'No response'}")
                export_errors += 1

        if export_errors > 0:
            print(f"\nERROR: {export_errors} exports failed!")
            sys.exit(1)
            
        if total_annotated == 0:
            print("\nERROR: No annotations found in any sample books!")
            sys.exit(1)

        # Step 4: Incremental Upgrade Check
        print_banner("STEP 4: INCREMENTAL UPGRADE (CACHE CHECK)")
        print("Sending 'upgrade' command. It should finish very quickly by skipping known archives.")
        send_command(process, "upgrade", {})
        
        while True:
            resp = read_response(process)
            if not resp: break
            status = resp.get("status")
            if status == "progress":
                pass # ignore progress spam for this quick check
            elif status == "ok":
                stats = resp.get('data', {})
                print(f"Upgrade Stats: {json.dumps(stats, indent=2)}")
                if stats.get('inserted', 0) > 0:
                    print("Warning: Expected 0 inserted books on second run, DB caching might be failing.")
                if stats.get('skipped', 0) == 0 and stats.get('archives_processed', 0) > 0:
                    # In new fast upgrade, skipped books are counted during pre-scanning
                    print("Error: Expected many skipped books (already in DB), got 0.")
                    sys.exit(1)
                if stats.get('archives_processed', 0) == 0:
                    print("Error: archives_processed is 0, database state lost?")
                    sys.exit(1)
                break
            else:
                print(f"\nUpgrade failed: {resp.get('error')}")
                sys.exit(1)
                
        # Final Export validation
        print_banner("STEP 5: VALIDATING EXPORTED BOOKS")
        validated_count = 0
        for f in export_dir.glob("*"):
            if f.is_file():
                # 1. Check size
                size = f.stat().st_size
                if size < 500:
                     print(f"Error: Exported file {f.name} is suspiciously small ({size} bytes)!")
                     sys.exit(1)
                
                # 2. Check content (FB2 should start with XML declaration or <FictionBook)
                try:
                    with open(f, 'rb') as fb2:
                        content_head = fb2.read(1000)
                        # We use 'ignore' because it might be CP1251
                        text_head = content_head.decode('utf-8', errors='ignore')
                        if "<FictionBook" not in text_head and "<?xml" not in text_head:
                            print(f"Error: Exported file {f.name} does not look like a valid FB2/XML file!")
                            sys.exit(1)
                        validated_count += 1
                except Exception as e:
                    print(f"Error reading exported file {f.name}: {e}")
                    sys.exit(1)
        
        if validated_count == 0:
            print("Error: No files found in export directory to validate!")
            sys.exit(1)
            
        print(f"Successfully validated {validated_count} exported files (size and basic structure check).")
        print("Exported files look valid.")

        print_banner("STEP 6: VALIDATING COVERS (META)")
        if not covers_dir.exists():
            print("Error: meta directory not created!")
            sys.exit(1)
        
        cover_files = list(covers_dir.rglob("cover.*"))
        if len(cover_files) == 0:
            print("Error: No covers found in the meta directory!")
            sys.exit(1)
            
        print(f"Found {len(cover_files)} covers. Checking sizes...")
        valid_covers = 0
        for f in cover_files[:10]: # Check up to 10 covers
            size = f.stat().st_size
            if size < 50:
                print(f"Error: Cover file {f} is suspiciously small ({size} bytes)!")
                sys.exit(1)
            valid_covers += 1
            
        print(f"Successfully validated {valid_covers} cover samples.")

    finally:
        print_banner("SHUTDOWN")
        if process.poll() is None:
            try:
                process.stdin.write("quit\n")
                process.stdin.flush()
                process.wait(timeout=5)
            except:
                process.terminate()

    print(f"\nTest passed! Logs: {log_path}")

if __name__ == "__main__":
    main()
