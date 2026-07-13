// test_asan.cpp — ASan test binary. One case per argv[1]; ASan cases abort
// their own process by design, so run_tests.sh runs each in isolation.
#include <Seeed_Arduino_SSCMA.h>

#include <string>

#include "test_harness.h"

// ===== happy path: begin() + invoke() parse =================================
static int case_happy() {
  fprintf(stderr, "== happy path ==\n");
  HardwareSerial fake;
  SSCMA ai;
  feed_begin(fake, "0000000012345678", "sscma");
  bool ok = ai.begin(&fake, -1, 921600, 2);
  TH_CHECK(ok, "begin() succeeds when ID?/NAME? are queued");
  TH_EQ_STR(fake.tx, "AT+ID?\r\nAT+NAME?\r\n");
  TH_EQ_STR(ai.ID(), "0000000012345678");
  TH_EQ_STR(ai.name(), "sscma");

  fake.clearTx();
  fake.feedReply(CMD_TYPE_RESPONSE, "INVOKE", 0, "{\"status\":0}");
  fake.feedReply(CMD_TYPE_EVENT, "INVOKE", 0,
                 "{\"perf\":[11,22,33],"
                 "\"boxes\":[[100,101,50,60,88,1],[10,11,5,6,77,2]],"
                 "\"classes\":[[95,3]],\"image\":\"QUJD\"}");
  TH_EQ_INT(ai.invoke(1, false, false), CMD_OK);
  TH_EQ_INT(ai.perf().inference, 22);
  TH_EQ_INT(ai.boxes().size(), 2);
  if (ai.boxes().size() == 2) {
    TH_EQ_INT(ai.boxes()[0].x, 100);
    TH_EQ_INT(ai.boxes()[1].target, 2);
  }
  TH_EQ_INT(ai.classes().size(), 1);
  if (ai.classes().size() == 1) TH_EQ_INT(ai.classes()[0].score, 95);
  TH_EQ_STR(ai.last_image().c_str(), "QUJD");
  TH_REPORT();
}

#if defined(__has_include)
#  if __has_include(<sanitizer/allocator_interface.h>)
#    include <sanitizer/allocator_interface.h>
#    define HAVE_ASAN_ALLOC 1
#  endif
#endif

// Leak: construct+destruct must net ~zero heap growth. Pre-fix ~SSCMA() is
// empty, so rx_buf(4097)+tx_buf(4096)=8193 bytes leak per instance.
static int case_ownership_leak() {
  fprintf(stderr, "== ownership: no leak on destruct ==\n");
#if HAVE_ASAN_ALLOC
  { HardwareSerial w; feed_begin(w); SSCMA* p = new SSCMA(); p->begin(&w,-1,921600,2); delete p; } // warm up
  size_t before = __sanitizer_get_current_allocated_bytes();
  { HardwareSerial f; feed_begin(f); SSCMA* ai = new SSCMA(); ai->begin(&f,-1,921600,2); delete ai; }
  long long delta = (long long)__sanitizer_get_current_allocated_bytes() - (long long)before;
  fprintf(stderr, "  delta=%+lld (pre-fix ~= +8193)\n", delta);
  TH_CHECK(delta < 4096, "construct+destruct must not leak; delta=%+lld", delta);
#else
  fprintf(stderr, "  SKIP: no ASan allocator interface\n");
#endif
  TH_REPORT();
}

// Destruct a never-begun object: destructor must free only NULL pointers.
// Pre-fix the members are uninitialized garbage; a real destructor freeing
// them would crash, so this guards ctor-init + destructor together.
static int case_ownership_destruct() {
  fprintf(stderr, "== ownership: destruct before begin ==\n");
  SSCMA* ai = new SSCMA();
  delete ai;  // must not crash / must not free garbage
  TH_CHECK(true, "destruct before begin is safe");
  TH_REPORT();
}

// NEW CASES ARE APPENDED HERE BY LATER TASKS.

int main(int argc, char** argv) {
  std::string which = argc > 1 ? argv[1] : "happy";
  if (which == "happy") return case_happy();
  if (which == "ownership_leak") return case_ownership_leak();
  if (which == "ownership_destruct") return case_ownership_destruct();
  // NEW DISPATCH ENTRIES ARE ADDED HERE BY LATER TASKS.
  fprintf(stderr, "unknown case: %s\n", which.c_str());
  return 2;
}
