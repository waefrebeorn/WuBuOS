#!/bin/bash
cd /home/wubu/wubunos
PASS=0
FAIL=0
BUILD_ERR=0
CRASH=0

for target in $(grep "^test_hw_" mk/tests.mk | sed 's/:.*//' | sort -u); do
    mod="${target#test_hw_}"
    rm -f "src/kernel/$target"
    
    make -B "$target" > /dev/null 2>&1
    if [ ! -f "src/kernel/$target" ]; then
        BUILD_ERR=$((BUILD_ERR + 1))
        echo "BUILD_ERR: $target"
        continue
    fi
    
    DISPLAY=:0 "src/kernel/$target" > /tmp/test_log_$$ 2>&1
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