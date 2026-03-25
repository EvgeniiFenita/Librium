#!/usr/bin/env python3
import os
import sys

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

import json
import time
import base64
import subprocess
import shutil
import socket
from pathlib import Path

# Add the scripts directory to path to import Core
sys.path.append(str(Path(__file__).parent))
from Core import CUI, CPaths, CRunner

class CSmokeTester:
    def __init__(self, binary_path, output_dir, real_lib_path):
        self.binary_path = Path(binary_path)
        self.output_dir = Path(output_dir)
        self.real_lib_path = Path(real_lib_path)
        
        if not self.real_lib_path.exists():
            CUI.error(f"Real library path {self.real_lib_path} does not exist.")
            sys.exit(1)

        # DETECT INPX FILE
        inpx_files = list(self.real_lib_path.glob("*.inpx"))
        if not inpx_files:
            CUI.error(f"No .inpx file found in {self.real_lib_path}")
            sys.exit(1)
        self.inpx_path = inpx_files[0]
        CUI.info(f"Detected INPX file: {self.inpx_path.name}")

        # DETECT ARCHIVES DIRECTORY
        # 1. Try standard 'lib.rus.ec' subfolder
        self.archives_dir = self.real_lib_path / "lib.rus.ec"
        
        # 2. If not found, look for any subfolder containing .zip files
        if not self.archives_dir.exists():
            for sub in self.real_lib_path.iterdir():
                if sub.is_dir() and any(sub.glob("*.zip")):
                    self.archives_dir = sub
                    break
        
        # 3. Fallback to root
        if not self.archives_dir or not self.archives_dir.exists():
            self.archives_dir = self.real_lib_path
        
        CUI.info(f"Detected archives directory: {self.archives_dir}")
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def _b64_encode(self, data):
        return base64.b64encode(data.encode('utf-8')).decode('utf-8')

    def _b64_decode(self, data):
        return base64.b64decode(data.encode('utf-8')).decode('utf-8')

    def _send_command(self, sock_file, action, params):
        cmd = {"action": action, "params": params}
        sock_file.write(self._b64_encode(json.dumps(cmd)) + "\n")
        sock_file.flush()

    def _read_response(self, sock_file):
        line = sock_file.readline().strip()
        if not line:
            return None
        try:
            return json.loads(self._b64_decode(line))
        except Exception as e:
            CUI.error(f"Protocol error: {e}. Line: {line}")
            return None

    def run(self):
        db_path = self.output_dir / "test.db"
        config_path = self.output_dir / "config.json"
        log_path = self.output_dir / "librium.log"
        export_dir = self.output_dir / "exported"

        CUI.info(f"Cleaning up old test data in {self.output_dir}...")
        for f in self.output_dir.glob("test.db*"):
            try: f.unlink()
            except: pass
        if export_dir.exists():
            shutil.rmtree(export_dir)
        covers_dir = self.output_dir / "meta"
        if covers_dir.exists():
            shutil.rmtree(covers_dir)

        # 1. Setup Config
        config = {
            "database": { "path": str(db_path) },
            "library": { 
                "inpxPath": str(self.inpx_path),
                "archivesDir": str(self.archives_dir)
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

        CUI.banner("REAL LIBRARY TEST: STARTING ENGINE (CLEAN RUN)")
        port = 50052
        process = subprocess.Popen(
            [str(self.binary_path), "--config", str(config_path), "--port", str(port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        sock = None
        for i in range(20):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(('127.0.0.1', port))
                break
            except ConnectionRefusedError:
                time.sleep(0.1)
        
        if not sock:
            CUI.error("Failed to connect to engine")
            process.terminate()
            return False
            
        sock_file = sock.makefile('rw', encoding='utf-8')

        try:
            # Step 1: Import
            CUI.banner("STEP 1: IMPORTING LIBRARY (CLEAN)")
            self._send_command(sock_file, "import", {})
            start_time = time.time()
            last_print_time = 0
            
            while True:
                resp = self._read_response(sock_file)
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
                        print(f"\rProgress: {processed}/{total} ({pct}%) | Speed: {speed:.0f} books/sec   ", end="", flush=True)
                        last_print_time = now
                elif status == "ok":
                    print("\nImport completed successfully.")
                    stats = resp.get('data', {})
                    CUI.info(f"Stats: {json.dumps(stats, indent=2)}")
                    if stats.get('fb2_parsed', 0) == 0 and stats.get('inserted', 0) > 0:
                        CUI.error("Clean import resulted in 0 FB2 parsed books!")
                        return False
                    break
                else:
                    CUI.error(f"Import failed: {resp.get('error')}")
                    return False

            # Step 2.2: General Pagination
            CUI.banner("STEP 2.2: GENERAL PAGINATION")
            total_annotated = 0
            all_retrieved_books = []
            for offset in [0, 50, 100]:
                self._send_command(sock_file, "query", {"limit": 50, "offset": offset})
                resp = self._read_response(sock_file)
                if resp and resp.get("status") == "ok":
                    books = resp.get("data", {}).get("books", [])
                    all_retrieved_books.extend(books)
                    annotated = sum(1 for b in books if b.get("annotation"))
                    total_annotated += annotated
                    CUI.info(f"Offset {offset}: Retrieved {len(books)} books. With annotations: {annotated}.")
                else:
                    CUI.error(f"Query failed: {resp.get('error') if resp else 'No response'}")
                    return False

            if all_retrieved_books:
                import random
                CUI.info("--- DETAILED DATA VERIFICATION (10 Random Samples) ---")
                samples = random.sample(all_retrieved_books, min(10, len(all_retrieved_books)))
                for i, b in enumerate(samples, 1):
                    print(f"\n[SAMPLE {i}/10]")
                    for key, val in b.items():
                        if key == "annotation" and val:
                            clean_val = val.replace('\n', ' ')
                            preview = (clean_val[:200] + '...') if len(clean_val) > 200 else clean_val
                            print(f"  {key:15}: {preview}")
                        else:
                            print(f"  {key:15}: {val}")

            CUI.banner("STEP 2.3: DYNAMIC SEARCH VERIFICATION")
            
            search_author = "Стругацки" # Default fallback
            if all_retrieved_books:
                # Pick a random book that has authors
                books_with_authors = [b for b in all_retrieved_books if b.get('authors')]
                if books_with_authors:
                    sample_book = random.choice(books_with_authors)
                    search_author = sample_book['authors'][0].get('lastName', "Стругацкий")
            
            # Use prefix search (first 5-6 chars) to test partial matching
            search_term = search_author[:6]
            CUI.info(f"Performing search for author prefix: '{search_term}' (picked from random sample)")
            
            self._send_command(sock_file, "query", {"author": search_term, "limit": 10})
            resp = self._read_response(sock_file)
            books_for_export = []
            if resp and resp.get("status") == "ok":
                books = resp.get("data", {}).get("books", [])
                total = resp.get('data', {}).get('totalFound')
                CUI.info(f"Found {total} books matching '{search_term}'.")
                for b in books[:10]:
                    books_for_export.append(b)
                    authors = b.get('authors', [])
                    auth_str = ", ".join([f"{a.get('firstName')} {a.get('lastName')}" for a in authors])
                    print(f"  - {b.get('title')} [{auth_str}] (ID: {b.get('id')})")
            else:
                CUI.error(f"Search failed: {resp.get('error') if resp else 'No response'}")
                return False

            # Step 3: Export
            CUI.banner("STEP 3: EXPORTING BOOKS")
            export_dir.mkdir(exist_ok=True)
            export_errors = 0
            for idx, book in enumerate(books_for_export, 1):
                book_id = book.get("id")
                self._send_command(sock_file, "export", {"id": book_id, "out": str(export_dir)})
                resp = self._read_response(sock_file)
                if resp and resp.get("status") == "ok":
                    file_path = resp.get("data", {}).get("file")
                    if not file_path or not Path(file_path).exists():
                        CUI.error(f"[{idx}/10] Export failed: file not found at {file_path}")
                        export_errors += 1
                        continue
                    size = Path(file_path).stat().st_size
                    CUI.info(f"[{idx}/10] Exported ID {book_id}: {size / 1024:.1f} KB")
                else:
                    CUI.error(f"[{idx}/10] Export failed for ID {book_id}: {resp.get('error') if resp else 'No response'}")
                    export_errors += 1

            if export_errors > 0:
                CUI.error(f"{export_errors} exports failed!")
                return False
                
            if total_annotated == 0:
                CUI.error("No annotations found in any sample books!")
                return False

            # Step 4: Incremental Upgrade Check
            CUI.banner("STEP 4: INCREMENTAL UPGRADE (CACHE CHECK)")
            CUI.info("Sending 'upgrade' command. It should finish very quickly by skipping known archives.")
            self._send_command(sock_file, "upgrade", {})
            
            while True:
                resp = self._read_response(sock_file)
                if not resp: break
                status = resp.get("status")
                if status == "progress":
                    pass
                elif status == "ok":
                    stats = resp.get('data', {})
                    CUI.info(f"Upgrade Stats: {json.dumps(stats, indent=2)}")
                    if stats.get('inserted', 0) > 0:
                        CUI.info("Warning: Expected 0 inserted books on second run, DB caching might be failing.")
                    if stats.get('skipped', 0) == 0 and stats.get('archives_processed', 0) > 0:
                        CUI.error("Expected many skipped books (already in DB), got 0.")
                        return False
                    if stats.get('archives_processed', 0) == 0:
                        CUI.error("archives_processed is 0, database state lost?")
                        return False
                    break
                else:
                    CUI.error(f"Upgrade failed: {resp.get('error')}")
                    return False
                    
            # Final Export validation
            CUI.banner("STEP 5: VALIDATING EXPORTED BOOKS")
            validated_count = 0
            for f in export_dir.glob("*"):
                if f.is_file():
                    size = f.stat().st_size
                    if size < 500:
                         CUI.error(f"Exported file {f.name} is suspiciously small ({size} bytes)!")
                         return False
                    
                    try:
                        with open(f, 'rb') as fb2:
                            content_head = fb2.read(1000)
                            text_head = content_head.decode('utf-8', errors='ignore')
                            if "<FictionBook" not in text_head and "<?xml" not in text_head:
                                CUI.error(f"Exported file {f.name} does not look like a valid FB2/XML file!")
                                return False
                            validated_count += 1
                    except Exception as e:
                        CUI.error(f"Error reading exported file {f.name}: {e}")
                        return False
            
            if validated_count == 0:
                CUI.error("No files found in export directory to validate!")
                return False
                
            CUI.info(f"Successfully validated {validated_count} exported files.")

            CUI.banner("STEP 6: VALIDATING COVERS (META)")
            if not covers_dir.exists():
                CUI.error("meta directory not created!")
                return False
            
            cover_files = list(covers_dir.rglob("cover.*"))
            if len(cover_files) == 0:
                CUI.error("No covers found in the meta directory!")
                return False
                
            CUI.info(f"Found {len(cover_files)} covers. Checking sizes...")
            valid_covers = 0
            for f in cover_files[:10]:
                size = f.stat().st_size
                if size < 50:
                    CUI.error(f"Cover file {f} is suspiciously small ({size} bytes)!")
                    return False
                valid_covers += 1
                
            CUI.info(f"Successfully validated {valid_covers} cover samples.")
            return True

        finally:
            CUI.banner("SHUTDOWN")
            if process.poll() is None:
                try:
                    if sock_file:
                        sock_file.write(self._b64_encode(json.dumps({"action": "quit", "params": {}})) + "\n")
                        sock_file.flush()
                    process.wait(timeout=5)
                except:
                    process.terminate()
                finally:
                    if sock: sock.close()

def main():
    if len(sys.argv) < 4:
        print("Usage: SmokeTester.py <binary_path> <output_dir> <real_library_path>")
        sys.exit(1)

    test = CSmokeTester(sys.argv[1], sys.argv[2], sys.argv[3])
    success = test.run()
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
