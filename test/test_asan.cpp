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

// A >31-char ID must be truncated into char _ID[32], never overflow it.
// Pre-fix: strcpy overruns the object; a 512-char ID runs off the heap block
// and ASan aborts. Post-fix: truncated, no abort.
static int case_overflow_id() {
  fprintf(stderr, "== bounded ID copy ==\n");
  HardwareSerial fake;
  SSCMA* ai = new SSCMA();
  std::string longid(512, 'A');
  fake.feedReply(0, "ID?", 0, "\"" + longid + "\"");
  fake.feedReply(0, "NAME?", 0, "\"sscma\"");
  ai->begin(&fake, -1, 921600, 2);   // pre-fix: ASan heap-buffer-overflow here
  char* id = ai->ID();
  TH_CHECK(id != NULL, "ID present");
  TH_CHECK(strlen(id) <= 31, "ID truncated to fit _ID[32], got %zu", strlen(id));
  delete ai;
  TH_REPORT();
}

// A RESPONSE whose data is missing must not strcpy(dst, NULL) / crash.
static int case_nullkey() {
  fprintf(stderr, "== missing data key is safe ==\n");
  HardwareSerial fake;
  SSCMA ai;
  feed_begin(fake);
  ai.begin(&fake, -1, 921600, 2);
  // ID? reply with no "data" field at all.
  fake.feedFrame("{\"type\":0,\"name\":\"ID?\",\"code\":0}");
  char* id = ai.ID(false);           // pre-fix: strcpy(_ID, NULL) -> crash
  TH_CHECK(id == NULL, "ID() returns NULL when data is absent");
  TH_REPORT();
}

// Over-length WIFI/MQTT fields must not overflow their fixed buffers.
static int case_wifi_mqtt_overflow() {
  fprintf(stderr, "== bounded WIFI/MQTT copies ==\n");
  HardwareSerial fake;
  SSCMA ai;
  feed_begin(fake);
  ai.begin(&fake, -1, 921600, 2);
  std::string big(300, 'x');
  fake.feedReply(CMD_TYPE_RESPONSE, "WIFI?", 0,
                 "{\"status\":1,\"config\":{\"security\":0,\"name\":\"" + big +
                 "\",\"password\":\"" + big + "\"}}");
  wifi_t w{};
  int r = ai.WIFI(w);                // pre-fix: strcpy overflows ssid[64]
  TH_CHECK(r == CMD_OK, "WIFI parsed");
  TH_CHECK(strlen(w.ssid) < sizeof(w.ssid), "ssid within bounds");
  TH_CHECK(strlen(w.password) < sizeof(w.password), "password within bounds");
  TH_REPORT();
}

// An INVOKE event with no "name" must not crash praser_event()'s strstr.
static int case_event_noname() {
  fprintf(stderr, "== event with no name is safe ==\n");
  HardwareSerial fake;
  SSCMA ai;
  feed_begin(fake);
  ai.begin(&fake, -1, 921600, 2);
  fake.feedReply(CMD_TYPE_RESPONSE, "INVOKE", 0, "{\"status\":0}");
  fake.feedFrame("{\"type\":1,\"code\":0,\"data\":{\"boxes\":[[1,2,3,4,5,6]]}}"); // no name
  int r = ai.invoke(1, false, false);
  TH_CHECK(r == CMD_ETIMEDOUT || r == CMD_OK, "no crash on nameless event (got %d)", r);
  TH_REPORT();
}

// set_tx_buffer(small) then an SPI command must not overrun tx_buf: spi_cmd
// transfers PACKET_SIZE bytes and writes tx_buf[4+len]. Pre-fix ASan aborts.
static int case_clamp_tx() {
  fprintf(stderr, "== tx buffer clamped to >= PACKET_SIZE ==\n");
  SPIClass spi;
  SSCMA* ai = new SSCMA();
  // SPI begin() issues a RESET spi_cmd; give it no sync pin. It will also try
  // ID()/name() which time out (no MISO script) — that's fine, we only care
  // that spi_cmd doesn't overrun.
  ai->begin(&spi, -1, -1, -1, 15000000, 0);
  bool ok = ai->set_tx_buffer(16);   // deliberately tiny
  TH_CHECK(ok, "set_tx_buffer(16) succeeds (clamped)");
  // Drive an SPI command path through write() -> spi_write -> spi_cmd.
  const char payload[8] = {1,2,3,4,5,6,7,8};
  ai->write(payload, sizeof(payload));  // pre-fix: heap-buffer-overflow in spi_cmd
  TH_CHECK(true, "spi_cmd did not overrun tx_buf");
  delete ai;
  TH_REPORT();
}

// Before any successful fetch, ID(cache=true) must return NULL, not a bogus
// empty cached string. Pre-fix `if (cache && _ID)` is always true.
//
// A transport is attached (as every other case does) but no ID?/NAME? reply
// is queued, so begin() times out and _ID is never populated -- this keeps
// the case isolated to the cache-guard bug. A truly transport-less `SSCMA
// ai;` (no begin() at all) is not usable here: write() has no "no transport
// configured" branch and falls through to i2c_write(), dereferencing a NULL
// _wire. That is a pre-existing bug independent of this one (name(), whose
// guard was already correct, crashes the same way on a never-begun object)
// and is out of scope for this fix.
static int case_id_cache() {
  fprintf(stderr, "== ID() cache guard ==\n");
  HardwareSerial fake;
  SSCMA ai;
  ai.begin(&fake, -1, 921600, 2);   // no reply queued: ID?/NAME? time out, _ID stays ""
  char* id = ai.ID(true);           // pre-fix: returns _ID (""), not NULL
  TH_CHECK(id == NULL, "uncached ID() must return NULL, got '%s'", id ? id : "(null)");
  TH_REPORT();
}

// NEW CASES ARE APPENDED HERE BY LATER TASKS.

int main(int argc, char** argv) {
  std::string which = argc > 1 ? argv[1] : "happy";
  if (which == "happy") return case_happy();
  if (which == "ownership_leak") return case_ownership_leak();
  if (which == "ownership_destruct") return case_ownership_destruct();
  if (which == "overflow_id") return case_overflow_id();
  if (which == "nullkey") return case_nullkey();
  if (which == "wifi_mqtt_overflow") return case_wifi_mqtt_overflow();
  if (which == "event_noname") return case_event_noname();
  if (which == "clamp_tx") return case_clamp_tx();
  if (which == "id_cache") return case_id_cache();
  // NEW DISPATCH ENTRIES ARE ADDED HERE BY LATER TASKS.
  fprintf(stderr, "unknown case: %s\n", which.c_str());
  return 2;
}
