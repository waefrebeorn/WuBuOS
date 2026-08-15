#!/bin/bash
# Run all test_hw_* targets, leveraging cached objects for fast builds.
# Binaries now live in build/testbin/ (post-reorg).
cd /home/wubu/wubunos
mkdir -p build/testbin
PASS=0
FAIL=0
BUILD_ERR=0
CRASH=0

for target in $(grep "^test_hw_" mk/tests.mk | sed 's/:.*//' | sort -u); do
    mod="${target#test_hw_}"
    
    make "$target" > /dev/null 2>&1
    if [ ! -f "build/testbin/$target" ]; then
        BUILD_ERR=$((BUILD_ERR + 1))
        echo "BUILD_ERR: $target"
        continue
    fi
    
    DISPLAY=:0 "build/testbin/$target" > /tmp/test_log_$$ 2>&1
    rc=$?
    if [ $rc -eq 139 ] || [ $rc -eq 134 ]; then
        CRASH=$((CRASH + 1))
        echo "CRASH($rc): $target"
    elif [ $rc -ne 0 ]; then
        FAIL=$((FAIL + 1))
        echo "FAIL: $target (exit=$rc)"
    else
        PASS=$((PASS + 1))
    fi
done

echo ""
echo "=== RESULTS ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "BUILD_ERR: $BUILD_ERR"
echo "CRASH: $CRASH"
echo "TOTAL: $((PASS + FAIL + BUILD_ERR + CRASH))"
