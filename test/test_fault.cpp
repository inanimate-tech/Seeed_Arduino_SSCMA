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

// If buffer allocation fails inside begin(), begin() must return false and not
// proceed to touch the (NULL) buffers. The SPI path is where this is fatal:
// begin() issues spi_cmd(RESET) which writes tx_buf[0] with no NULL check.
static int case_begin_alloc_fail() {
  fprintf(stderr, "== begin() bails when buffer alloc fails ==\n");
  SPIClass spi;
  SSCMA ai;
  fault::fail_malloc_after(0);   // every malloc in begin() fails
  bool ok = ai.begin(&spi, -1, -1, -1, 15000000, 0);
  fault::disarm();
  TH_CHECK(!ok, "begin() must return false when buffers can't allocate");
  TH_REPORT();  // reaching here at all = no crash (pre-fix: spi_cmd writes tx_buf[0]=NULL -> SIGSEGV)
}

// NEW CASES ARE APPENDED HERE BY LATER TASKS.

// When the payload malloc fails mid-parse, wait() must not spin forever: it
// must fall back to the outer timeout and return CMD_ETIMEDOUT. Watchdog
// catches the pre-fix infinite loop.
static int case_oom_wait() {
  fprintf(stderr, "== wait() terminates when payload malloc fails ==\n");
  HardwareSerial fake;
  feed_begin(fake);
  SSCMA ai;
  ai.begin(&fake, -1, 921600, 2);
  // Queue a full RESPONSE frame, then fail every allocation and invoke.
  fake.feedReply(CMD_TYPE_RESPONSE, "INVOKE", 0, "{\"status\":0}");
  arm_watchdog(3);
  fault::fail_malloc_after(0);
  int r = ai.invoke(1, false, false);  // wait() runs here
  fault::disarm();
  alarm(0);
  TH_CHECK(r == CMD_ETIMEDOUT, "invoke/wait must time out, not hang (got %d)", r);
  TH_REPORT();
}

// fetch() has no timeout at all; on malloc failure it must break out of the
// frame loop rather than spin. It returns void, so success = it returns and
// the callback is never invoked.
static int case_oom_fetch() {
  fprintf(stderr, "== fetch() terminates when payload malloc fails ==\n");
  HardwareSerial fake;
  feed_begin(fake);
  SSCMA ai;
  ai.begin(&fake, -1, 921600, 2);
  fake.feedReply(CMD_TYPE_EVENT, "INVOKE", 0, "{\"count\":1}");
  bool called = false;
  arm_watchdog(3);
  fault::fail_malloc_after(0);
  ai.fetch([&](const char*, size_t) { called = true; });
  fault::disarm();
  alarm(0);
  TH_CHECK(!called, "callback must not fire when payload alloc failed");
  TH_CHECK(true, "fetch() returned (did not hang)");
  TH_REPORT();
}

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
  if (which == "begin_alloc_fail") return case_begin_alloc_fail();
  if (which == "oom_wait") return case_oom_wait();
  if (which == "oom_fetch") return case_oom_fetch();
  // NEW DISPATCH ENTRIES ARE ADDED HERE BY LATER TASKS.
  fprintf(stderr, "unknown case: %s\n", which.c_str());
  return 2;
}
