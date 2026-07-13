// Must FAIL to compile once SSCMA's copy ctor/assignment are deleted.
#include <Seeed_Arduino_SSCMA.h>
void must_not_compile() {
  SSCMA a;
  SSCMA b = a;   // copy-construct — deleted
  SSCMA c; c = a; // copy-assign — deleted
  (void)b; (void)c;
}
