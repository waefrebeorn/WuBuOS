#!/usr/bin/env python3
"""A .c file was split: does every build list name the new pieces?

Three times in one session a split module broke the build the same way: the code was split, the
makefile line that names the old file was not, and the failure surfaced only after a 7m40s suite as
`undefined reference to <helper>`. A 30-second grep would have said it first.

Rule this enforces: if a source file F is named in any build recipe or object list, then every
source that F was split into must be named TOO, next to it. The split is declared in
tools/split_links.json, so the check needs no guessing:

    { "src/runtime/wubu_jpeg.c": ["src/runtime/wubu_jpeg_core.c",
                                  "src/runtime/wubu_jpeg_prog.c"] }

Exit 1 if any file lists the parent without all of its parts.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "tools/split_links.json"


def variants(path):
    """The ways a makefile may spell a source: literal, $(RT)/, basename .o."""
    p = pathlib.Path(path)
    stem = p.stem
    return [path, "$(RT)/" + p.name, stem + ".o", "$(RT)/" + stem + ".o", str(p.with_suffix(".o"))]


def main():
    if not MANIFEST.exists():
        print("split_links: no manifest, nothing to check")
        return 0
    splits = {k: v for k, v in json.loads(MANIFEST.read_text()).items() if not k.startswith("_")}
    files = sorted(list((ROOT / "mk").glob("*.mk")) + list((ROOT).glob("Makefile")))
    bad = 0
    for parent, parts in splits.items():
        pv = variants(parent)
        for mf in files:
            text = mf.read_text(errors="replace")
            # look line by line: a recipe may name the parent without the parts
            for ln in text.splitlines():
                if not any(v in ln for v in pv):
                    continue
                missing = [p for p in parts if not any(v in ln for v in variants(p))]
                if missing:
                    bad += 1
                    print("  SPLIT NOT LINKED: %s names %s but not %s"
                          % (mf.relative_to(ROOT), pathlib.Path(parent).name,
                             ", ".join(pathlib.Path(m).name for m in missing)))
    if bad:
        print("split_links: %d line(s) name a split parent without all its parts" % bad)
        return 1
    print("split_links: every declared split is fully named wherever its parent is (%d split(s))"
          % len(splits))
    return 0


if __name__ == "__main__":
    sys.exit(main())
