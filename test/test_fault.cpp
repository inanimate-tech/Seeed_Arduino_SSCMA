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

// A failed re-size (realloc) must leave the object fully usable: the old
// buffer stays owned, rx_len unchanged, setter returns false. Pre-fix the
// setter nulls rx_buf while leaving rx_len set, so the next invoke() writes
// through NULL and crashes.
static int case_realloc_fail() {
  fprintf(stderr, "== realloc failure keeps object usable ==\n");
  HardwareSerial fake;
  feed_begin(fake);
  SSCMA ai;
  bool ok = ai.begin(&fake, -1, 921600, 2);
  TH_CHECK(ok, "clean begin");

  fault::fail_realloc_after(0);          // next realloc returns NULL
  bool grew = ai.set_rx_buffer(8 * 1024);
  fault::disarm();
  TH_CHECK(!grew, "set_rx_buffer must report failure when realloc fails");

  // Object must still work: drive a full invoke through the original buffer.
  fake.clearTx();
  fake.feedReply(CMD_TYPE_RESPONSE, "INVOKE", 0, "{\"status\":0}");
  fake.feedReply(CMD_TYPE_EVENT, "INVOKE", 0, "{\"boxes\":[[1,2,3,4,5,6]]}");
  int r = ai.invoke(1, false, false);    // pre-fix: NULL write -> crash
  TH_CHECK(r == CMD_OK, "invoke still works after a failed resize (got %d)", r);
  TH_EQ_INT(ai.boxes().size(), 1);
  TH_REPORT();
}

int main(int argc, char** argv) {
  std::string which = argc > 1 ? argv[1] : "";
  if (which == "realloc_fail") return case_realloc_fail();
  // NEW DISPATCH ENTRIES ARE ADDED HERE BY LATER TASKS.
  fprintf(stderr, "unknown case: %s\n", which.c_str());
  return 2;
}
