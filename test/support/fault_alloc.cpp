// fault_alloc.cpp — strong malloc/realloc overrides. Validated on Apple
// clang: a plain strong definition in the executable is picked up for the
// entire image without ASan or dyld interposition. Do NOT define a free()
// override: libc's free correctly releases the calloc-backed blocks these
// overrides return, and overriding free() here would self-recurse (libc++
// aliases std::free to ::free).
#include "fault_alloc.h"

#include <cstring>
#include <cstdlib>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#define FAULT_USABLE_SIZE(p) malloc_size(p)
#else
#include <malloc.h>
#define FAULT_USABLE_SIZE(p) malloc_usable_size(p)
#endif

namespace {
int g_malloc_fail_after = -1;
int g_realloc_fail_after = -1;
long g_malloc_count = 0;
long g_realloc_count = 0;
}  // namespace

namespace fault {
void fail_malloc_after(int n) { g_malloc_fail_after = n; g_malloc_count = 0; }
void fail_realloc_after(int n) { g_realloc_fail_after = n; g_realloc_count = 0; }
void disarm() { g_malloc_fail_after = -1; g_realloc_fail_after = -1; }
}  // namespace fault

// Internal real allocation. calloc is NOT overridden here, so using it avoids
// recursing into our own malloc.
static void* real_alloc(size_t n) { return calloc(1, n ? n : 1); }

extern "C" void* malloc(size_t n) {
  if (g_malloc_fail_after >= 0 && g_malloc_count++ >= g_malloc_fail_after)
    return nullptr;
  return real_alloc(n);
}

extern "C" void* realloc(void* p, size_t n) {
  if (g_realloc_fail_after >= 0 && g_realloc_count++ >= g_realloc_fail_after)
    return nullptr;  // original block left intact & owned — caller must keep it
  if (!p) return malloc(n);
  void* q = real_alloc(n);
  if (q) {
    size_t old = FAULT_USABLE_SIZE(p);
    memcpy(q, p, n < old ? n : old);  // bounded: never over-read past p
  }
  free(p);  // libc free (no override) releases the calloc-backed block
  return q;
}
