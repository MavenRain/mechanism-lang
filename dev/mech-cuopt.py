#!/usr/bin/env python3
"""Run the checked source package without installing it into Python."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from mechanism_cuopt.cli import main

raise SystemExit(main())
