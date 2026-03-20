#!/usr/bin/env python3
import os
import json
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
        
        for sf in sorted(scenario_files):
            if "smoke_real" in sf and not self.real_lib_path:
                print(f"Skipping {sf} (Real library path not provided)")
                continue
                
            res = self.run_scenario(sf)
            results.append(res)
            
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
        process = subprocess.Popen(
            [self.binary_path, "--config", config_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding='utf-8',
            errors='ignore',
            bufsize=1
        )
        
        # 4. Execute Steps
        last_output = ""
        try:
            for step in data["steps"]:
                success, output = self._execute_step_engine(process, step, test_dir, db_path, lib_path, config_path)
                if not success:
                    process.terminate()
                    return {"status": "FAIL"}
                last_output = output
                
                # Per-step validation
                if "expected" in step or "expected_text" in step:
                    if not self._validate(step, last_output, inpx_count):
                        process.terminate()
                        return {"status": "FAIL"}

            # 5. Final Validation (backwards compatibility)
            if "expected" in data or "expected_text" in data:
                if not self._validate(data, last_output, inpx_count):
                    process.terminate()
                    return {"status": "FAIL"}
        finally:
            if process.poll() is None:
                try:
                    process.stdin.write("quit\n")
                    process.stdin.flush()
                except:
                    process.terminate()

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
                "inpxPath": os.path.join(lib_path, "library.inpx") if "library" in data else os.path.join(lib_path, "librusec_local_fb2.inpx"),
                "archivesDir": lib_path if "library" in data else os.path.join(lib_path, "lib.rus.ec")
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

    def _execute_step_engine(self, process, step, test_dir, db_path, lib_path, config_path):
        action, params = self._map_args_to_json(step["args"], db_path, lib_path, config_path, test_dir)
        cmd = {"action": action, "params": params}
        
        process.stdin.write(self._b64_encode(json.dumps(cmd)) + "\n")
        process.stdin.flush()
        
        has_progress = False
        while True:
            line = process.stdout.readline().strip()
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
                    # For query, we expect data to be the actual result
                    if action == "query":
                        return True, json.dumps(resp.get("data"))
                    return True, json.dumps(resp)
                    
                else: # error or unknown
                    if has_progress: print()
                    print(f" FAIL\n--- ACTION FAILED: {action} ---\nERROR: {resp.get('error', 'Unknown error')}")
                    return False, ""
                    
            except Exception as e:
                if has_progress: print()
                print(f" FAIL\n--- PROTOCOL ERROR: {e} ---\nLINE: {line}")
                return False, ""

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

    def _validate(self, data, last_output, inpx_count):
        if "expected" in data:
            expected_str = json.dumps(data["expected"]).replace("\"{INPX_COUNT}\"", str(inpx_count))
            expected = json.loads(expected_str)
            try:
                actual = json.loads(last_output)
                
                # Decision: what to compare?
                # If expected has "status", compare the whole root.
                # If actual has "data" and expected matches keys in "data", compare data.
                
                to_compare = actual
                if "status" not in expected and "data" in actual and not isinstance(actual["data"], list):
                    # Check if expected keys are in actual["data"]
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
        return True

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
