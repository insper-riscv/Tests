/* Self-contained malloc()/free(), used instead of picolibc's.
 *
 * Tests link against the toolchain's picolibc (toolchain.libc in
 * config.yaml), but this object is on the link command line, so the
 * malloc() and free() below win over picolibc's own, which would need a
 * _sbrk() and a heap this project's link scripts don't provide.
 *
 * A plain bump allocator is enough for this project's tests: none of
 * them build/tear down repeatedly or care about reclaiming memory
 * mid-test, so free() is a no-op rather than a real free-list.
 *
 * Heap grows up from _bss_end (link.ld/golden.ld's own end-of-.bss
 * symbol — already exists, no new linker symbol needed) towards
 * _stack_top (also already defined there) — returns NULL on
 * exhaustion, same convention libc's own malloc uses.
 */
#include <stddef.h>

extern char _bss_end;
extern char _stack_top;

static char *heap_ptr = NULL;

void *malloc(size_t size) {
    if (heap_ptr == NULL) {
        heap_ptr = &_bss_end;
    }
    /* 4-byte-align every allocation -- struct fields on RV32 expect
     * natural alignment, and a byte-granular bump pointer would
     * eventually hand back a misaligned block otherwise. */
    size = (size + 3u) & ~(size_t)3u;

    if (heap_ptr + size > &_stack_top) {
        return NULL;
    }
    char *block = heap_ptr;
    heap_ptr += size;
    return block;
}

void free(void *ptr) {
    (void)ptr; /* bump allocator -- nothing to reclaim, see header comment */
}
