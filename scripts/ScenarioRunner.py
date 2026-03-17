#!/usr/bin/env python3
import os
import json
import subprocess
import shutil
import glob
import zipfile
import sys
import time

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

    def run_scenario(self, scenario_file):
        """Executes a single scenario workflow."""
        name = os.path.basename(scenario_file).replace(".json", "")
        
        with open(scenario_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        description = data.get("description", "No description provided")
        print(f"Running scenario: {name} ({description})...", end="", flush=True)
        
        start_time = time.time()
        test_dir = self._prepare_test_dir(name)
        
        # Paths
        db_path = os.path.join(test_dir, "test.db")
        config_path = os.path.join(test_dir, "config.json")
        
        # 1. Setup Data
        lib_path, inpx_count = self._setup_library(data, test_dir)
        
        # 2. Setup Config
        self._write_config(data, config_path, db_path, lib_path)
        
        # 3. Execute Steps
        last_output = ""
        for step in data["steps"]:
            success, output = self._execute_step(step, test_dir, db_path, lib_path, config_path)
            if not success:
                return {"status": "FAIL"}
            last_output = output
            
            # Per-step validation
            if "expected" in step or "expected_text" in step:
                if not self._validate(step, last_output, inpx_count):
                    return {"status": "FAIL"}

        # 4. Final Validation (backwards compatibility)
        if "expected" in data or "expected_text" in data:
            if not self._validate(data, last_output, inpx_count):
                return {"status": "FAIL"}

        duration = time.time() - start_time
        print(f" OK ({duration:.2f}s)")
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
            "logging": { "level": "info", "file": os.path.join(os.path.dirname(config_path), "librium.log") }
        }
        if "config" in data:
            for section, settings in data["config"].items():
                if section not in config: config[section] = {}
                config[section].update(settings)
        
        with open(config_path, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2)

    def _execute_step(self, step, test_dir, db_path, lib_path, config_path):
        cmd_args = self._prepare_args(step["args"], db_path, lib_path, config_path, test_dir)
        
        res = subprocess.run(cmd_args, capture_output=True, text=True, encoding='utf-8', errors='ignore')
        if res.returncode != 0:
            print(f" FAIL\n--- COMMAND FAILED: {' '.join(cmd_args)} ---\nEXIT CODE: {res.returncode}\nSTDERR: {res.stderr}")
            return False, ""
        
        output = res.stdout
        if step["args"][0] == "query":
            try:
                idx = cmd_args.index("--output")
                with open(cmd_args[idx+1], 'r', encoding='utf-8') as f:
                    output = f.read()
            except: pass
            
        return True, output

    def _prepare_args(self, args, db_path, lib_path, config_path, test_dir):
        cmd_name = args[0]
        cmd_args = [self.binary_path] + args
        
        if cmd_name in ["import", "upgrade"]:
            if "--config" not in cmd_args: cmd_args += ["--config", config_path]
        elif cmd_name in ["query", "stats", "export"]:
            if "--db" not in cmd_args: cmd_args += ["--db", db_path]
            if cmd_name == "query" and "--output" not in cmd_args:
                cmd_args += ["--output", os.path.join(test_dir, "query_out.json")]
            if cmd_name == "export":
                if "--archives" not in cmd_args: cmd_args += ["--archives", lib_path]
                if "--out" not in cmd_args: cmd_args += ["--out", os.path.join(test_dir, "exported")]
        elif cmd_name == "init-config":
            if "--output" not in cmd_args: cmd_args += ["--output", os.path.join(test_dir, "new_config.json")]

        return [a.replace("{LIB}", lib_path).replace("{DB}", db_path).replace("{CONFIG}", config_path) if isinstance(a, str) else a for a in cmd_args]

    def _validate(self, data, last_output, inpx_count):
        if "expected" in data:
            expected_str = json.dumps(data["expected"]).replace("\"{INPX_COUNT}\"", str(inpx_count))
            expected = json.loads(expected_str)
            try:
                actual = json.loads(last_output)
                if not self._compare(actual, expected):
                    print(f" FAIL (Mismatch)\nEXPECTED: {json.dumps(expected)}\nACTUAL: {json.dumps(actual)}")
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
