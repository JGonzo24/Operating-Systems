#include "include/malloc.h"
#include "include/pp.h"
// Macros
#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define HDR_SIZE ALIGN(sizeof(header_t))

#define PAGE_SIZE (64 * 1024)
#define PAYLOAD(h) ((void *)((char *)(h) + HDR_SIZE))
#define HDR_FROM_PAYLOAD(p) ((header_t *)((char *)(p) - HDR_SIZE))

// Global vars
char *heap_start = NULL;
char *heap_end = NULL;
header_t *free_list = NULL;

// LOGGING
static int debug_malloc_enabled = -1;
static int log_busy = 0; // 0 = free, 1 = logging

static void debug_log(const char *fmt, ...)
{
    // Enable only if DEBUG_MALLOC is set in the environment
    if (debug_malloc_enabled == -1)
    {
        const char *v = getenv("DEBUG_MALLOC");
        debug_malloc_enabled = (v && *v) ? 1 : 0;
    }
    if (!debug_malloc_enabled)
        return;

    // Reentrancy guard: if we’re already in debug_log, bail out
    if (log_busy)
        return;
    log_busy = 1;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
        write(STDERR_FILENO, buf, len);
    }

    log_busy = 0;
}

static int grow_heap(size_t min_bytes)
{
    if (min_bytes < (HDR_SIZE + ALIGNMENT))
    {
        min_bytes = HDR_SIZE + ALIGNMENT;
    }

    // Round up to PAGE_SIZE
    size_t pages = (min_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t grow = pages * PAGE_SIZE;

    void *old_end = sbrk(grow);
    if (old_end == (void *)-1)
    {
        debug_log("MALLOC: grow_heap(%zu) failed (sbrk)\n", min_bytes);
        return -1;
    }
    // New free block starts at previous heap_end
    header_t *h = (header_t *)((char *)old_end);
    h->is_used = false;
    h->next = NULL;
    h->size = grow - HDR_SIZE;

    // Update heap_end
    heap_end = (char *)old_end + grow;

    // Insert and let insert_free_block coalesce if adjacent
    insert_free_block(h);

    debug_log("MALLOC: grow_heap(%zu) => +%zu bytes @%p\n",
              min_bytes, grow, (void *)h);
    return 0;
}

// Helper Functions
int init_heap(void)
{
    // Initalize the heap with 64k bytes
    void *base = sbrk(PAGE_SIZE);
    if (base == (void *)-1)
    {
        pp(stdout, "init_heap failure\n");
        return -1;
    }

    heap_start = (char *)base;
    heap_end = heap_start + PAGE_SIZE;
    header_t *h = (header_t *)heap_start;

    size_t usable = PAGE_SIZE - HDR_SIZE;
    h->is_used = false;
    h->next = NULL;
    h->size = usable;

    free_list = h;
    return 0;
}

/**
 * REALLOC helper functions
 */
static inline char *block_end(const header_t *h)
{
    return (char *)h + HDR_SIZE + h->size;
}

static inline header_t *next_block(const header_t *h)
{
    char *n = block_end(h);
    if (n >= heap_end)
        return NULL;
    return (header_t *)n;
}

void unlink_block(header_t *target)
{
    // Look for the target, remove from the list
    header_t **link = &free_list;
    header_t *curr = free_list;

    while (curr)
    {
        if (curr == target)
        {
            *link = curr->next;
            return;
        }
        link = &curr->next;
        curr = curr->next;
    }
}

/**
 * REALLOC case 1: Shrink in place
 */
void shrink_block(header_t *h, size_t asize)
{
    if (h->size <= asize)
        return;

    size_t left = h->size - asize;

    if (left < (HDR_SIZE + ALIGNMENT))
    {
        return; // Too small
    }

    // Create a new free block at the tail
    header_t *tail = (header_t *)((char *)h + HDR_SIZE + asize);
    tail->size = left - HDR_SIZE;
    tail->is_used = false;
    tail->next = NULL;

    // Shrink current block
    h->size = asize;
    insert_free_block(tail);
}
/**
 * REALLOC case 2: Expand in place
 */
bool try_expand(header_t *h, size_t asize) {
    if (h->size >= asize) return true;

    header_t *next = next_block(h);
    if (!next || next->is_used) return false;

    // total bytes available after removing the boundary header
    size_t combined = h->size + HDR_SIZE + next->size;

    if (combined < asize) return false;

    // We can expand. Remove `next` from the free list.
    unlink_block(next);

    // If there’s enough room to leave a tail free block, split it.
    if (combined - asize >= HDR_SIZE + ALIGNMENT) {
        header_t *tail = (header_t *)((char *)h + HDR_SIZE + asize);
        tail->size = (combined - asize) - HDR_SIZE;
        tail->is_used = false;
        tail->next = NULL;

        h->size = asize;
        insert_free_block(tail);
    } else {
        // Just take it all
        h->size = combined;
    }
    return true;
}

/**
 * This is going to insert the freed
 * block back into the free list
 */
void insert_free_block(header_t *h)
{
    header_t *prev = NULL;
    header_t *curr = free_list;

    // Find place to insert the free block
    while (curr && curr < h)
    {
        prev = curr;
        curr = curr->next;
    }

    // Put between previous and current
    h->next = curr;
    if (prev)
        prev->next = h;
    else
        free_list = h;

    // See if you can merge adgacent memory
    if (h->next && adjacent_mem(h, h->next))
    {
        header_t *n = h->next;
        h->size += HDR_SIZE + n->size;
        h->next = n->next;
    }

    if (prev && adjacent_mem(prev, h))
    {
        prev->size += HDR_SIZE + h->size;
        prev->next = h->next;
    }
}

/**
 * We are now starting free
 * - Need to insert into the free list
 * - Update pointers if adjacent blocks are free
 */
void free(void *ptr)
{
    debug_log("MALLOC: free(%p)\n", ptr);

    if (!ptr)
    {
        debug_log("MALLOC: free(NULL) - ignoring\n");
        return;
    }

    // Basic bounds check
    if ((char *)ptr < heap_start + HDR_SIZE || (char *)ptr >= heap_end)
    {
        debug_log("MALLOC: free(%p) - invalid pointer (out of heap bounds)\n", ptr);
        return;
    }

    header_t *h = HDR_FROM_PAYLOAD(ptr);
    h->is_used = false;
    insert_free_block(h);
}

/**
 * MALLOC()
 * This is going to request 64K bytes from the OS
 * Move the break 64K bytes up
 */
void *malloc(size_t size)
{
    if (size == 0)
    {
        pp(stdout, "MALLOC IS NULL");
        return NULL;
    }
    // Ensure heap initialization
    if (!heap_start)
    {
        if (init_heap() != 0)
        {
            pp(stdout, "init heap err");
            return NULL;
        }
    }

    size_t asize = ALIGN(size);
    header_t **plink = find_fit(asize);

    if (!plink)
    {
        size_t need = HDR_SIZE + asize;
        if (grow_heap(need) == 0)
        {
            plink = find_fit(asize);
        }
    }

    if (!plink)
    {
        // Out of memory even after growth
        debug_log("MALLOC: OOM malloc(%zu)\n", size);
        return NULL;
    }
    // Now have the new header pointer
    // be at the same place where theres space
    header_t *h = *plink;

    h = split_block(h, asize);

    // Update the list to be the next free
    *plink = h->next;

    // The current header is now used!
    h->next = NULL;

    h->is_used = true;

    void *ret = PAYLOAD(h);
    debug_log("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n", size, ret, size);
    return ret;
}

void *calloc(size_t nmemb, size_t size)
{
    // 0-size policy (consistent with your malloc)
    if (nmemb == 0 || size == 0)
    {

        return NULL;
    }

    // overflow check: nmemb * size
    if (size > SIZE_MAX / nmemb)
    {
        return NULL;
    }

    size_t total = nmemb * size;

    void *p = malloc(total);
    if (!p)
    {
        debug_log("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)\n",
                  nmemb, size, p, total);
        return NULL;
    }
    // zero exactly the requested bytes
    memset(p, 0, total);
    debug_log("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)\n",
              nmemb, size, p, total);
    return p;
}
