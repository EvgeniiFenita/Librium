#!/usr/bin/env python3
import sys
import os

# Prevent creation of __pycache__ folders
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

from pathlib import Path
sys.path.append(str(Path(__file__).parent))
from Core import CRunner

if __name__ == "__main__":
    CRunner.run_python("Run.py", ["build"] + sys.argv[1:])
