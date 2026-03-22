#!/usr/bin/env python3
import os
import json
import socket
import subprocess
import shutil
import glob
import zipfile
import sys
import time
import base64

# Prevent creation of __pycache__ folders in source directories
sys.dont_write_bytecode = True

from LibGen import LibGen

class ScenarioRunner:
    """Test scenario runner with step-by-step execution and validation."""
    
    def __init__(self, binary_path, output_root, real_lib_path=None):
        self.binary_path = binary_path
        self.output_root = os.path.abspath(output_root)
        self.real_lib_path = real_lib_path
        os.makedirs(self.output_root, exist_ok=True)

    def run_all(self, scenarios_dir):
        """Runs all scenarios from the specified directory."""
        scenario_files = glob.glob(os.path.join(scenarios_dir, "**/*.json"), recursive=True)
        results = []
        skipped = 0
        
        for sf in sorted(scenario_files):
            if "smoke_real" in sf and not self.real_lib_path:
                print(f"Skipping {sf} (Real library path not provided)")
                skipped += 1
                continue
                
            res = self.run_scenario(sf)
            results.append(res)
            
        passed = sum(1 for r in results if r["status"] == "PASS")
        failed = sum(1 for r in results if r["status"] == "FAIL")
        
        print("=" * 80)
        print(f"  SCENARIO SUMMARY: {passed} passed, {failed} failed, {skipped} skipped (Total: {len(scenario_files)})")
        print("=" * 80)
            
        return results

    def _b64_encode(self, data):
        return base64.b64encode(data.encode('utf-8')).decode('utf-8')

    def _b64_decode(self, data):
        return base64.b64decode(data.encode('utf-8')).decode('utf-8')

    def run_scenario(self, scenario_file):
        """Executes a single scenario workflow."""
        name = os.path.basename(scenario_file).replace(".json", "")
        
        with open(scenario_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        description = data.get("description", "No description provided")
        print(f"Running scenario: {name} ({description})...")
        
        start_time = time.time()
        test_dir = self._prepare_test_dir(name)
        
        # Paths
        db_path = os.path.join(test_dir, "test.db")
        config_path = os.path.join(test_dir, "config.json")
        
        # 1. Setup Data
        lib_path, inpx_count = self._setup_library(data, test_dir)
        
        # 2. Setup Config
        self._write_config(data, config_path, db_path, lib_path)
        
        # 3. Start Engine
        port = self._find_free_port()
        process = subprocess.Popen(
            [self.binary_path, "--config", config_path, "--port", str(port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        
        def _connect_to_engine(attempts=30):
            for _ in range(attempts):
                try:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.connect(('127.0.0.1', port))
                    return s
                except ConnectionRefusedError:
                    time.sleep(0.1)
            return None

        sock = _connect_to_engine()
        if not sock:
            process.terminate()
            return {"status": "FAIL", "error": "Failed to connect to engine"}
            
        sock_file = sock.makefile('rw', encoding='utf-8')
        
        # 4. Execute Steps
        last_output = ""
        try:
            for step in data["steps"]:
                # Reconnect before this step if requested (e.g. after a connection-closing step)
                if step.get("reconnect"):
                    try:
                        sock_file.close()
                        sock.close()
                    except Exception:
                        pass
                    sock = _connect_to_engine()
                    if not sock:
                        process.terminate()
                        print(f" FAIL\n--- Failed to reconnect to engine after step ---")
                        return {"status": "FAIL", "error": "Failed to reconnect to engine"}
                    sock_file = sock.makefile('rw', encoding='utf-8')

                success, output = self._execute_step_engine(process, sock, sock_file, step, test_dir, db_path, lib_path, config_path)
                if not success:
                    process.terminate()
                    return {"status": "FAIL"}
                last_output = output
                
                # Per-step validation
                if "expected" in step or "expected_text" in step or \
                   "expected_file_exists" in step or "expected_file_contains" in step:
                    if not self._validate(step, last_output, inpx_count, lib_path, test_dir):
                        process.terminate()
                        return {"status": "FAIL"}

            # 5. Final Validation (backwards compatibility)
            if "expected" in data or "expected_text" in data or \
               "expected_file_exists" in data or "expected_file_contains" in data:
                if not self._validate(data, last_output, inpx_count, lib_path, test_dir):
                    process.terminate()
                    return {"status": "FAIL"}
        finally:
            if process.poll() is None:
                try:
                    if sock_file:
                        sock_file.write("quit\n")
                        sock_file.flush()
                except Exception:
                    process.terminate()
                finally:
                    try:
                        if sock: sock.close()
                    except Exception:
                        pass

        duration = time.time() - start_time
        print(f"  --> OK ({duration:.2f}s)\n")
        return {"status": "PASS"}

    def _prepare_test_dir(self, name):
        test_dir = os.path.join(self.output_root, name)
        if os.path.exists(test_dir):
            try: shutil.rmtree(test_dir)
            except: pass
        os.makedirs(test_dir, exist_ok=True)
        return test_dir

    def _setup_library(self, data, test_dir):
        lib_gen = LibGen(test_dir)
        lib_path = ""
        inpx_count = 0
        
        if "library" in data:
            for b in data["library"]:
                lib_gen.add_book(**b)
            lib_path = lib_gen.generate()
            inpx_count = len(data["library"])
        elif "real_library" in data:
            lib_path = self.real_lib_path
            inpx_file = os.path.join(lib_path, "librusec_local_fb2.inpx")
            inpx_count = self._count_inpx_records(inpx_file)
            
        return lib_path, inpx_count

    def _write_config(self, data, config_path, db_path, lib_path):
        config = {
            "database": { "path": db_path },
            "library": { 
                "inpxPath": os.path.join(lib_path, "library.inpx") if "library" in data else
                            os.path.join(lib_path, "librusec_local_fb2.inpx") if lib_path else "",
                "archivesDir": lib_path if "library" in data else
                               os.path.join(lib_path, "lib.rus.ec") if lib_path else ""
            },
            "import": { "threadCount": 8, "parseFb2": False },
            "logging": { "level": "debug", "file": os.path.join(os.path.dirname(config_path), "librium.log") }
        }
        if "config" in data:
            for section, settings in data["config"].items():
                if section not in config: config[section] = {}
                config[section].update(settings)
        
        with open(config_path, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2)

    def _execute_step_engine(self, process, sock, sock_file, step, test_dir, db_path, lib_path, config_path):
        action, params = self._map_args_to_json(step["args"], db_path, lib_path, config_path, test_dir)
        expect_error = step.get("expect_error", False)

        # Special action: send raw (unencoded) bytes — used for protocol error tests
        if action == "send_raw":
            return self._execute_raw_send(sock, params.get("data", ""), expect_error)

        cmd = {"action": action, "params": params}
        
        sock_file.write(self._b64_encode(json.dumps(cmd)) + "\n")
        sock_file.flush()
        
        has_progress = False
        while True:
            line = sock_file.readline().strip()
            if not line:
                return False, "Process exited prematurely"
                
            try:
                resp_str = self._b64_decode(line)
                resp = json.loads(resp_str)
                
                status = resp.get("status")
                
                if status == "progress":
                    data = resp.get("data", {})
                    processed = data.get("processed", 0)
                    total = data.get("total", 0)
                    pct = (processed * 100 // total) if total > 0 else 0
                    print(f"\r    Progress: {processed}/{total} ({pct}%)", end="", flush=True)
                    has_progress = True
                    continue
                    
                elif status == "ok":
                    if has_progress:
                        print()
                    if expect_error:
                        print(f" FAIL\n--- Expected error response but got ok ---")
                        return False, ""
                    # For query, we expect data to be the actual result
                    if action == "query":
                        return True, json.dumps(resp.get("data"))
                    return True, json.dumps(resp)
                    
                else:  # error or unknown
                    if has_progress: print()
                    if expect_error:
                        return True, json.dumps(resp)
                    print(f" FAIL\n--- ACTION FAILED: {action} ---\nERROR: {resp.get('error', 'Unknown error')}")
                    return False, ""
                    
            except Exception as e:
                if has_progress: print()
                print(f" FAIL\n--- PROTOCOL ERROR: {e} ---\nLINE: {line}")
                return False, ""

    def _execute_raw_send(self, sock, raw_data, expect_close_or_error):
        """Send raw (unencoded) bytes and accept an error response or connection close."""
        try:
            sock.sendall((raw_data + "\n").encode('utf-8'))
        except OSError as e:
            print(f" FAIL\n--- Could not send raw data: {e} ---")
            return False, ""

        # Try to read the response with a short timeout
        old_timeout = sock.gettimeout()
        sock.settimeout(5.0)
        buf = b""
        try:
            while b"\n" not in buf:
                chunk = sock.recv(4096)
                if not chunk:
                    # Server closed the connection — acceptable for malformed input
                    if expect_close_or_error:
                        return True, json.dumps({"status": "connection_closed"})
                    print(f" FAIL\n--- Server closed connection unexpectedly ---")
                    return False, ""
                buf += chunk

            line = buf.split(b"\n")[0].strip()
            resp_str = self._b64_decode(line.decode('utf-8'))
            resp = json.loads(resp_str)

            if resp.get("status") == "error" and expect_close_or_error:
                return True, json.dumps(resp)
            if not expect_close_or_error:
                return True, json.dumps(resp)
            # Got ok when we expected an error — test failure
            print(f" FAIL\n--- Expected error/close for raw send, got: {resp} ---")
            return False, ""

        except (socket.timeout, TimeoutError):
            # No response — treat as connection closed
            if expect_close_or_error:
                return True, json.dumps({"status": "connection_closed"})
            print(f" FAIL\n--- Timeout waiting for response to raw send ---")
            return False, ""
        except Exception as e:
            if expect_close_or_error:
                return True, json.dumps({"status": "connection_closed"})
            print(f" FAIL\n--- Error reading raw send response: {e} ---")
            return False, ""
        finally:
            sock.settimeout(old_timeout)

    def _map_args_to_json(self, args, db_path, lib_path, config_path, test_dir):
        action = args[0]
        params = {}
        
        i = 1
        while i < len(args):
            arg = args[i]
            if arg.startswith("--"):
                key = arg[2:]
                if i + 1 < len(args) and not args[i+1].startswith("--"):
                    params[key] = args[i+1]
                    i += 2
                else:
                    params[key] = True
                    i += 1
            else:
                i += 1

        # Replace placeholders and convert types
        for k, v in params.items():
            if isinstance(v, str):
                v = v.replace("{LIB}", lib_path).replace("{DB}", db_path).replace("{CONFIG}", config_path)
                # Try to convert to int if it looks like a number
                if v.isdigit():
                    params[k] = int(v)
                else:
                    params[k] = v

        return action, params

    def _validate(self, data, last_output, inpx_count, lib_path="", test_dir=""):
        if "expected" in data:
            expected_str = json.dumps(data["expected"]).replace("\"{INPX_COUNT}\"", str(inpx_count))
            expected = json.loads(expected_str)
            try:
                actual = json.loads(last_output)
                
                to_compare = actual
                if "status" not in expected and "data" in actual and not isinstance(actual["data"], list):
                    if all(k in actual["data"] for k in expected.keys()):
                        to_compare = actual["data"]

                if not self._compare(to_compare, expected):
                    print(f" FAIL (Mismatch)\nEXPECTED: {json.dumps(expected)}\nACTUAL: {json.dumps(to_compare)}")
                    return False
            except Exception as e:
                print(f" FAIL (Invalid JSON: {e})\nRAW: {last_output}")
                return False

        if "expected_text" in data:
            for text in data["expected_text"]:
                if text not in last_output:
                    print(f" FAIL (Text not found: '{text}')\nRAW: {last_output}")
                    return False

        if "expected_file_exists" in data:
            # Extract a file path from a dot-separated JSON path in the response
            # e.g. "expected_file_exists": "data.cover"
            json_path = data["expected_file_exists"]
            try:
                actual = json.loads(last_output)
                file_path = self._extract_json_path(actual, json_path)
                if not file_path:
                    print(f" FAIL (expected_file_exists: field '{json_path}' is empty or missing)\nRAW: {last_output}")
                    return False
                if not os.path.isfile(file_path):
                    print(f" FAIL (expected_file_exists: file not found on disk: '{file_path}')")
                    return False
                if os.path.getsize(file_path) == 0:
                    print(f" FAIL (expected_file_exists: file is empty: '{file_path}')")
                    return False
            except Exception as e:
                print(f" FAIL (expected_file_exists error: {e})\nRAW: {last_output}")
                return False

        if "expected_file_contains" in data:
            # Check that a file at a given path contains the specified text.
            # "path"      — static path with {LIB}/{TEST_DIR} placeholders
            # "path_from" — dot-notation JSON path into the response (e.g. "data.file")
            spec = data["expected_file_contains"]
            search_text = spec.get("text", "")
            if "path_from" in spec:
                try:
                    actual = json.loads(last_output)
                    file_path = self._extract_json_path(actual, spec["path_from"])
                    if not file_path:
                        print(f" FAIL (expected_file_contains: field '{spec['path_from']}' is empty)\nRAW: {last_output}")
                        return False
                except Exception as e:
                    print(f" FAIL (expected_file_contains path_from error: {e})\nRAW: {last_output}")
                    return False
            else:
                raw_path = spec.get("path", "")
                file_path = raw_path.replace("{LIB}", lib_path).replace("{TEST_DIR}", test_dir)
            try:
                if not os.path.isfile(file_path):
                    print(f" FAIL (expected_file_contains: file not found: '{file_path}')")
                    return False
                with open(file_path, 'r', encoding='utf-8', errors='replace') as fh:
                    content = fh.read()
                if search_text not in content:
                    print(f" FAIL (expected_file_contains: '{search_text}' not found in '{file_path}')")
                    return False
            except Exception as e:
                print(f" FAIL (expected_file_contains error: {e})")
                return False

        return True

    def _extract_json_path(self, obj, path):
        """Extract a value from nested dict using dot-notation path (e.g. 'data.cover')."""
        for key in path.split("."):
            if not isinstance(obj, dict) or key not in obj:
                return None
            obj = obj[key]
        return obj

    def _compare(self, actual, expected, key=None):
        if isinstance(expected, dict):
            for k, v in expected.items():
                if k not in actual or not self._compare(actual[k], v, k): return False
            return True
        elif isinstance(expected, list):
            if len(actual) < len(expected): return False
            for e in expected:
                if not any(self._compare(a, e) for a in actual): return False
            return True
        
        if key == "totalFound" and isinstance(actual, int) and isinstance(expected, int):
            if expected == 0: return actual == 0
            delta = abs(actual - expected)
            return delta <= (expected * 0.01) + 10
                
        return actual == expected

    @staticmethod
    def _find_free_port():
        """Find a free TCP port on localhost."""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(('', 0))
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            return s.getsockname()[1]

    def _count_inpx_records(self, inpx_path):
        count = 0
        SEP = "\x04"
        try:
            with zipfile.ZipFile(inpx_path, 'r') as z:
                for name in z.namelist():
                    if name.lower().endswith('.inp'):
                        with z.open(name) as f:
                            for line in f:
                                try:
                                    line_str = line.decode('utf-8').strip()
                                    if not line_str: continue
                                    fields = line_str.split(SEP)
                                    if len(fields) > 8 and fields[8] == "0": count += 1
                                except: continue
        except Exception as e:
            print(f" WARN (Could not count INPX records: {e})")
        return count

if __name__ == "__main__":
    import sys
    runner = ScenarioRunner(sys.argv[1], sys.argv[2], sys.argv[4] if len(sys.argv) > 4 else None)
    results = runner.run_all(sys.argv[3])
    sys.exit(1 if any(r["status"] == "FAIL" for r in results) else 0)
