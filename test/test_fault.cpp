// test_fault.cpp — fault-injection binary (NO ASan). Linked with
// support/fault_alloc.cpp. One case per argv[1].
#include <Seeed_Arduino_SSCMA.h>

#include <csignal>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "support/fault_alloc.h"
#include "test_harness.h"

// Watchdog: if a case hangs (pre-fix infinite loop), SIGALRM kills it non-zero.
static void arm_watchdog(unsigned secs) {
  signal(SIGALRM, [](int) {
    const char m[] = "  WATCHDOG: case hung (timeout) -> FAIL\n";
    write(2, m, sizeof(m) - 1);
    _exit(3);
  });
  alarm(secs);
}

// NEW CASES ARE APPENDED HERE BY LATER TASKS.

int main(int argc, char** argv) {
  std::string which = argc > 1 ? argv[1] : "";
  // NEW DISPATCH ENTRIES ARE ADDED HERE BY LATER TASKS.
  fprintf(stderr, "unknown case: %s\n", which.c_str());
  return 2;
}
