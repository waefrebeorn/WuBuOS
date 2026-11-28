#!/usr/bin/env python3
"""disk_hygiene -- fail the suite when the workspace is eating the disk.

Why this exists: two runs today wrote 33 GB and 32 GB of log because a command's
unbounded output was redirected to a file. Deleting them fixed the day; this
prevents the next one by making the condition a red gate instead of a surprise.

Rules (all measured, none inferred):
  R1 free space on the tree's filesystem must stay above FLOOR_GB
  R2 no single file under the scratch/tmp roots may exceed FILE_MAX_GB
  R3 the total of those roots must stay under ROOT_TOTAL_GB

Exit 1 on any violation, and print the offending paths so the fix is obvious.
"""
import os
import sys

FLOOR_GB = float(os.environ.get('WUBU_DISK_FLOOR_GB', '20'))
FILE_MAX_GB = float(os.environ.get('WUBU_DISK_FILE_MAX_GB', '2'))
ROOT_TOTAL_GB = float(os.environ.get('WUBU_DISK_ROOT_TOTAL_GB', '40'))

ROOTS = [p for p in (
    os.environ.get('BH_AGENT_WORKSPACE'),
    os.path.expanduser('~/.hermes/cache/scratch'),
    '/tmp',
) if p and os.path.isdir(p)]

GB = 1024.0 ** 3


def free_gb(path):
    st = os.statvfs(path)
    return st.f_bavail * st.f_frsize / GB


def walk_sizes(root):
    """Yield (size, path) for regular files, skipping other filesystems."""
    root_dev = os.stat(root).st_dev
    stack = [root]
    while stack:
        d = stack.pop()
        try:
            with os.scandir(d) as it:
                for e in it:
                    try:
                        if e.is_dir(follow_symlinks=False):
                            if e.stat(follow_symlinks=False).st_dev == root_dev:
                                stack.append(e.path)
                        elif e.is_file(follow_symlinks=False):
                            yield e.stat(follow_symlinks=False).st_size, e.path
                    except OSError:
                        continue
        except OSError:
            continue


def main():
    fails = []
    print('disk_hygiene: checking %d root(s)' % len(ROOTS))

    for root in ROOTS:
        free = free_gb(root)
        print('  %-46s free %.1f GB' % (root, free))
        if free < FLOOR_GB:
            fails.append('FREE SPACE %.1f GB < %.0f GB at %s' % (free, FLOOR_GB, root))

        big, total = [], 0.0
        for size, path in walk_sizes(root):
            total += size
            if size > FILE_MAX_GB * GB:
                big.append((size, path))
        print('  %-46s holds %.1f GB' % (root, total / GB))
        if total > ROOT_TOTAL_GB * GB:
            fails.append('ROOT TOTAL %.1f GB > %.0f GB at %s' % (total / GB, ROOT_TOTAL_GB, root))
        for size, path in sorted(big, reverse=True)[:5]:
            fails.append('FILE %.1f GB > %.0f GB: %s' % (size / GB, FILE_MAX_GB, path))

    if fails:
        print('disk_hygiene: FAIL')
        for f in fails:
            print('  ' + f)
        print('  bound the writer: pipe long output through `tail -c 2000000`, or cap it with')
        print('  `( ulimit -f 102400; cmd ) > log` so the kernel stops it at 50 MB.')
        return 1

    print('disk_hygiene: PASS -- space and file sizes within budget')
    return 0


if __name__ == '__main__':
    sys.exit(main())
