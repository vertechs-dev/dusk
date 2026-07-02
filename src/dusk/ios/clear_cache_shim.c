// iOS's compiler-rt builtins archive does not provide __clear_cache (Apple's
// supported API is sys_icache_invalidate), but funchook's code patcher calls
// __builtin___clear_cache, which lowers to an external __clear_cache call.
// macOS's compiler-rt ships the symbol, so only iOS needs this bridge.
#include <libkern/OSCacheControl.h>
#include <stddef.h>

void __clear_cache(void* start, void* end) {
    sys_icache_invalidate(start, (size_t)((char*)end - (char*)start));
}
