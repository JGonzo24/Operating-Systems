/**
 * @file malloc.c
 * @author Joshua Gonzalez
 * @brief Assignment 1, CSC 453
 * @date 2025-09-29
 * 
 * 
 */
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>

/**
 * @struct header_t
 * @brief This struct defines what is in the header of the chunk
 */
typedef struct header{
    bool is_used;
    size_t size;
    struct header *next;
} header_t;

/**
 * @brief global variables
 */
static bool debug_enabled = false;
bool heap_initialized = false;
header_t *heap_head = NULL;
header_t *heap_tail = NULL;

/**
 * @brief User defines and macros
 *
 */
#define PAGE_SIZE (64*1024)
#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1)) 
#define HDR_SIZE ALIGN(sizeof(header_t))
#define PAYLOAD_FROM_HDR(h)  ((void *)((char *)(h) + HDR_SIZE))
#define HDR_FROM_PAYLOAD(p)  ((header_t *)((char *)(p) - HDR_SIZE))


/**
 * @brief function declarations
 */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
header_t *new_page(void);




/**
 * @brief Helper function declarations
 */
bool debug_malloc(void);
void log_msg(const char *str, ...);

bool debug_malloc(void)
{
    if (debug_enabled == false)
    {
        const char *v = getenv("DEBUG_MALLOC");
        debug_enabled = (v && *v) ? true : false;
    }
    return debug_enabled;
}

// Small, reusable helpers
static inline size_t ceil_div(size_t x, size_t y) {
    return (x + y - 1) / y;        // ceil(x / y)
}
static inline size_t round_up(size_t x, size_t m) {
    return ceil_div(x, m) * m;     // round x up to multiple of m
}

/**
 * @brief Uses vsnprintf() for debug printing
 * 
 * @param str 
 * @param ... 
 */
void log_msg(const char *str, ...)
{
    if (!debug_malloc())
        return;

    va_list args;
    va_start(args, str);
    char buf[256];

    int n = vsnprintf(buf, sizeof(buf), str, args);
    va_end(args);

    if(n > 0)
    {
        // Write the max number of chars 
        size_t len = (n < sizeof(buf)) ? n : sizeof(buf) - 1;
        write(STDERR_FILENO, buf, len);
    }
}

void split_block(header_t *h, size_t requested)
{
    if (h->size < requested)
        return;

    size_t remainder = h->size - requested;
    if (remainder < HDR_SIZE + ALIGNMENT)
    {
        return;
    }

    char *base = (char *)h;
    header_t *new_h = (header_t *)(base + HDR_SIZE + requested);

    new_h->is_used = false;
    new_h->next = NULL;
    new_h->size = remainder - HDR_SIZE;

    h->size = requested;
    h->next = new_h;

    if (heap_tail == h)
        heap_tail = new_h;

    
    log_msg("[split] %p -> used(%zu) + free(%p,%zu)\n",
            (void*)h, requested, (void*)new_h, new_h->size);
}

bool align_brk(void)
{
    void *cur_brk = sbrk(0);
    if (cur_brk == (void*)-1)
        return false;

    uintptr_t ptr = (uintptr_t)cur_brk;
    size_t pad = (size_t)(ALIGN(ptr) - ptr);
    // Move the brk to be aligned
    if (pad && sbrk((uintptr_t)pad) == (void*)-1)
        return false;
    return true;
}

header_t *grow_heap(size_t min_payload)
{
    if (!align_brk())
    {
        log_msg("grow heap: align failed\n");
        return NULL;
    }

    // Round up to how many bytes you need
    size_t bytes = round_up(HDR_SIZE + ALIGN(min_payload), PAGE_SIZE);

    void *base = sbrk((uintptr_t)(bytes));
    if (base == (void*)-1)
    {
        log_msg("grow heap: sbrk failed\n");
        return NULL;
    }

    header_t *new_hdr = (header_t *)base;
    new_hdr->is_used = false;
    new_hdr->next = NULL;
    new_hdr->size = bytes - HDR_SIZE;

    if (!heap_head)
    {
        heap_head = heap_tail = new_hdr;
        heap_initialized = true;
    }
    else
    {
        heap_tail->next = new_hdr;
        heap_tail = new_hdr;
    }

    log_msg("[grow heap] base=%p bytes=%zu payload=%zu\n", base, bytes, new_hdr->size);
    return new_hdr;
}

header_t *find_fit(size_t requested)
{
    for (header_t *h = heap_head; h; h=h->next)
    {
        if (!h->is_used && h->size >= requested)
        {
            log_msg("[find fit] found header=%p size=%zu need=%zu",
                    (void *)h, h->size, requested);
            return h;
        }
    }
    log_msg("[find_fit] no block large enough (need %zu)\n", requested);
    return NULL;
}

/**
 * @brief Return the pointer to the new head 
 * 
 * @param reqested 
 * @return header_t* 
 */
header_t *init_heap(size_t reqested)
{
    void *base = sbrk(PAGE_SIZE);
    
    if (base == (void *)-1)
    {
        log_msg("Error when using SBRK\n");
        return NULL;
    }

    uintptr_t base_ptr = (uintptr_t)base;

    // Offset 
    size_t pad = (size_t)(ALIGN(base_ptr) - base_ptr);

    header_t *base_header = (header_t *)(base_ptr + pad); 

    if(pad + HDR_SIZE > PAGE_SIZE)
    {
        // Not big enough!
        log_msg("After padding space not big enough\n");
        return NULL;
    }
    base_header->is_used = false;
    base_header->next = NULL;
    base_header->size = PAGE_SIZE - pad - HDR_SIZE;

    heap_head = heap_tail = base_header;
    heap_initialized = true;


    log_msg("[init_heap] base=%p pad=%zu hdr=%p payload=%zu\n",
            base, pad, (void *)base_header, base_header->size);

    return heap_head;
}

/**
 * @brief 
 * 
 * @param size 
 * @return void* 
 */
void* malloc(size_t size)
{
    if (size == 0)
    {
        log_msg("MALLOC: Size == 0\n");
        return NULL;
    }

    size_t requested = ALIGN(size);
    if(!heap_initialized)
    {
        if (!grow_heap(requested))
            return NULL;
    }
    header_t *new_h = find_fit(requested);
    if (!new_h)
    {
        if (!grow_heap(requested))
        {
            return NULL;
        }
        new_h = find_fit(requested);

        if (!new_h)
            return NULL;
    }
    split_block(new_h, requested);
    new_h->is_used = true;

    log_msg("MALLOC: malloc(%d)     => (ptr=%p, size=%d)\n",size, PAYLOAD_FROM_HDR(new_h), new_h->size);
    return PAYLOAD_FROM_HDR(new_h);
}

/**
 * @brief 
 * 
 * @param ptr 
 */
void free(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    if((uintptr_t)ptr % ALIGNMENT != 0)
    {
        log_msg("[free] unaligned payload %p ignored\n", ptr);
        return;
    }

    header_t *h = HDR_FROM_PAYLOAD(ptr);
    
    // Check if header is in the list
    bool header_in_list = false;
    for (header_t *curr = heap_head; curr; curr = curr->next)
    {
        if(curr == h)
        {
            header_in_list = true;
        }
    }
    if (header_in_list == false)
    {
        log_msg("[free] header not in list\n");
        return;
    }

    if(!h->is_used)
    {
        log_msg("[free] double free?");
        return;
    }

    h->is_used = false;
    log_msg("[free] %p ok (hdr=%p size =%zu)\n", ptr, (void *)h, h->size);
}

/**
 * @brief 
 * 
 * @param nmemb 
 * @param size 
 * @return void* 
 */
void *calloc(size_t nmemb, size_t size)
{
    // Match your malloc(0) behavior: return NULL on zero-sized requests
    if (nmemb == 0 || size == 0) {
        log_msg("[calloc] nmemb=%zu size=%zu -> NULL (zero-sized)\n", nmemb, size);
        return NULL;
    }

    // Overflow check: nmemb * size must not wrap
    if (size > SIZE_MAX / nmemb) {
        log_msg("[calloc] overflow: nmemb=%zu size=%zu\n", nmemb, size);
        return NULL;
    }

    size_t total = nmemb * size;

    void *p = malloc(total);
    if (!p) {
        log_msg("[calloc] malloc(%zu) failed\n", total);
        return NULL;
    }

    // Zero the allocated payload
    memset(p, 0, total);
    log_msg("[calloc] ok %zu bytes at %p\n", total, p);
    return p;
}

/**
 * @brief 
 * 
 * @param ptr 
 * @param size 
 * @return void* 
 */
void *realloc(void* ptr, size_t size)
{

}