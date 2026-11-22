#!/usr/bin/env python3
"""ci_status.py -- read boot-verify run status from GitHub's public API.

The HTML Actions page is CDN-cached and can show stale "In progress" rows;
the public API returns live status. No `gh` auth or token is needed for a
public repo, so this works on hosts where `gh` is not authenticated.

Usage: python3 tools/ci_status.py [workflow-yml]
Exit: 0 no failures in the recent runs, 1 a failed run, 2 fetch error.
"""
import json
import sys
import urllib.request

API = ("https://api.github.com/repos/waefrebeorn/WuBuOS/actions/workflows/"
       "{wf}/runs?per_page=10")


def parse_runs(data):
    """Return (number, status, conclusion, short_sha, title) tuples."""
    out = []
    for r in data.get("workflow_runs", []):
        out.append((r["run_number"], r["status"], r.get("conclusion"),
                    r["head_sha"][:7], r.get("display_title") or ""))
    return out


def failing_runs(rows):
    return [r for r in rows if r[2] == "failure"]


def main():
    wf = sys.argv[1] if len(sys.argv) > 1 else "boot-verify.yml"
    req = urllib.request.Request(
        API.format(wf=wf),
        headers={"Accept": "application/vnd.github+json",
                 "User-Agent": "wubu-ci-status"})
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            data = json.load(resp)
    except Exception as exc:                       # network / rate limit
        print("FAIL: cannot read CI status: %s" % exc)
        return 2
    rows = parse_runs(data)
    if not rows:
        print("FAIL: no workflow runs returned")
        return 1
    for n, status, conclusion, sha, title in rows:
        print("#%-4d %-12s %-10s %s %s" %
              (n, status, conclusion or "-", sha, title[:58]))
    bad = failing_runs(rows)
    if bad:
        print("CI: %d failed run(s) in the last %d" % (len(bad), len(rows)))
        return 1
    print("CI: no failures in the last %d runs" % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
