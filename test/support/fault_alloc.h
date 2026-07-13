// fault_alloc.h — controllable allocation-failure injection for the fault
// test binary. A STRONG malloc/realloc/free defined in the executable
// overrides libc for the whole linked image on macOS/Linux, including the
// separately-compiled library object. Do NOT link this into the ASan binary
// (ASan owns malloc). fault binary only.
#pragma once
namespace fault {
// Arm: fail the Nth (0-based) malloc/realloc after arming, then keep failing.
// n<0 disarms that call. disarm() clears both counters.
void fail_malloc_after(int n);
void fail_realloc_after(int n);
void disarm();
}  // namespace fault
