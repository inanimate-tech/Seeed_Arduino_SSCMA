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
# FAULT_CASES

# ---- compile-fail checks (Task 1) ----
# COMPILE_FAIL_CHECKS

if [ $fail -eq 0 ]; then echo "ALL CASES BEHAVED AS EXPECTED"; else echo "FAILURES ABOVE"; fi
exit $fail
