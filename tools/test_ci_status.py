#!/usr/bin/env python3
"""Unit gate for ci_status parsing (no network)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ci_status import parse_runs, failing_runs

SAMPLE = {"workflow_runs": [
    {"run_number": 116, "status": "in_progress", "conclusion": None,
     "head_sha": "90f154c7077145af983a42969ed2256731f7c511",
     "display_title": "console: drv"},
    {"run_number": 115, "status": "completed", "conclusion": "success",
     "head_sha": "8bf287f00000000000000000000000000000000",
     "display_title": "workflow: portable"},
]}

rows = parse_runs(SAMPLE)
assert len(rows) == 2, rows
assert rows[0][:4] == (116, "in_progress", None, "90f154c"), rows[0]
assert not failing_runs(rows), rows
failed = parse_runs({"workflow_runs": [
    {"run_number": 9, "status": "completed", "conclusion": "failure",
     "head_sha": "deadbeef00000000000000000000000000000000"}]})
assert len(failing_runs(failed)) == 1, failed
assert parse_runs({}) == []
print("test_ci_status: ALL PASS")
