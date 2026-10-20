#!/bin/sh
# test_ns_session.sh -- the 9P session test, built the way that actually works.
#
# The mk/tests.mk rule for this target resolves to a contaminated command list
# (make -n shows the 9P test binary), while the same commands run by hand link
# and pass. Rather than keep guessing at make's view of the rule, this script
# *is* the recipe: compile the test, link against the session sources plus the
# styx lock table, run it.
set -eu
RT=src/runtime
OBJ=$(mktemp /tmp/ns_session_XXXXXX.o)

gcc -O0 -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_HOSTED \
    -include wubu_gnu_compat.h -I"$RT" \
    -c "$RT/wubu_ns_session_test.c" -o "$OBJ"

gcc -O0 -no-pie -I"$RT" \
    "$RT/styx_locks.c" "$RT/wubu_ns_fs.o" \
    "$RT/wubu_ns_session.c" "$RT/wubu_session.c" \
    "$OBJ" -o "$RT/wubu_ns_session_test"

rm -f "$OBJ"
"$RT/wubu_ns_session_test"