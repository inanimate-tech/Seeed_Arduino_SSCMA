#!/usr/bin/env bash
# Drives both binaries. Each ASan/abort case runs one-per-process.
set -u
A=./build/test_asan
F=./build/test_fault
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1:allocator_may_return_null=1"
fail=0

pass() { # <bin> <case>
  echo "==== $2 (expect PASS) ===="
  "$1" "$2"; rc=$?
  [ $rc -ne 0 ] && { echo "!! $2 exited $rc, expected 0"; fail=1; }
  echo
}
asan_abort() { # <case> <needle>
  echo "==== $1 (expect ASan abort) ===="
  out=$("$A" "$1" 2>&1); rc=$?
  echo "$out" | tail -12
  echo "$out" | grep -q "$2" || { echo "!! $1: missing '$2'"; fail=1; }
  [ $rc -eq 0 ] && { echo "!! $1: exited 0, expected abort"; fail=1; }
  echo
}

# ---- registered by tasks (keep in task order) ----
pass "$A" happy
# ASAN_CASES
pass "$A" ownership_leak
pass "$A" ownership_destruct
pass "$F" realloc_fail
pass "$F" begin_alloc_fail
# FAULT_CASES

# ---- compile-fail checks (Task 1) ----
# COMPILE_FAIL_CHECKS
echo "==== compile_fail/copy_ctor (expect COMPILE ERROR) ===="
if clang++ -std=c++17 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -D__CORRECT_ISO_CPP_STRING_H_PROTO \
     -Istubs -Ithird_party/ArduinoJson/src -I../src -fsyntax-only compile_fail/copy_ctor.cpp 2>/dev/null; then
  echo "!! copy_ctor.cpp compiled — SSCMA copy is NOT deleted"; fail=1
else
  echo "ok: copying SSCMA is rejected at compile time"
fi
echo

if [ $fail -eq 0 ]; then echo "ALL CASES BEHAVED AS EXPECTED"; else echo "FAILURES ABOVE"; fi
exit $fail
