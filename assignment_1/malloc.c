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
#include <string.h>
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

/**
 * @brief Checks if DEBUG_MALLOC set 
 * 
 * @return true 
 * @return false 
 */
bool debug_malloc(void)
{
    if (debug_enabled == false)
    {
        const char *v = getenv("DEBUG_MALLOC");
        debug_enabled = (v && *v) ? true : false;
    }
    return debug_enabled;
}

/**
 * @brief Helper functions used for rounding
 * 
 * @param x 
 * @param y 
 * @return size_t 
 */
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

/**
 * @brief Split chunk
 * This function 
 * @param h 
 * @param requested 
 */
void split_block(header_t *h, size_t requested)
{
    // Can't split!
    if (h->size < requested)
        return;

    
    size_t remainder = h->size - requested;
    
    // Checking if remainder will fit in chunk
    if (remainder < HDR_SIZE + ALIGNMENT)
    {
        return;
    }

    char *base = (char *)h;
    // Calcualte the new header
    header_t *new_h = (header_t *)(base + HDR_SIZE + requested);

    new_h->is_used = false;
    new_h->next = h->next;
    new_h->size = remainder - HDR_SIZE;

    h->size = requested;
    h->next = new_h;

    if (heap_tail == h)
        heap_tail = new_h;
}

/**
 * @brief Ensuring that the sbrk() is aligned
 * 
 * @return true 
 * @return false 
 */
bool align_brk(void)
{
    void *cur_brk = sbrk(0);
    if (cur_brk == (void*)-1)
        return false;

    // Align 
    uintptr_t ptr = (uintptr_t)cur_brk;
    size_t pad = (size_t)(ALIGN(ptr) - ptr);
    // Move the brk to be aligned
    if (pad && sbrk((intptr_t)pad) == (void*)-1)
        return false;
    return true;
}

header_t *grow_heap(size_t min_payload)
{
    // Check if curr break is alligned
    if (!align_brk())
    {
        return NULL;
    }

    // Round up to how many bytes you need
    size_t bytes = round_up(HDR_SIZE + ALIGN(min_payload), PAGE_SIZE);

    // Move brk to alligned addr
    void *base = sbrk((intptr_t)(bytes));
    if (base == (void*)-1)
    {
        return NULL;
    }

    // Update metadata
    header_t *new_hdr = (header_t *)base;
    new_hdr->is_used = false;
    new_hdr->next = NULL;
    new_hdr->size = bytes - HDR_SIZE;

    // Check if this first initialization
    if (!heap_head)
    {
        heap_head = heap_tail = new_hdr;
        heap_initialized = true;
    }
    else
    {
        // Link the new headers
        heap_tail->next = new_hdr;
        heap_tail = new_hdr;
    }
    return new_hdr;
}

/**
 * @brief Finds a chunk to put the requested data
 * 
 * @param requested 
 * @return header_t* 
 */
header_t *find_fit(size_t requested)
{
    header_t *h;
    for (h = heap_head; h; h = h->next)
    {
        if (!h->is_used && h->size >= requested)
        {
            return h;
        }
    }
    return NULL;
}

/**
 * @brief MALLOC!
 * 
 * @param size 
 * @return void* 
 */
void* malloc(size_t size)
{
    if (size == 0)
    {
        log_msg("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n",
             size, (void*)NULL, (size_t)0);
        return NULL;
    }

    size_t requested = ALIGN(size);
    if(!heap_initialized)
    {
        // Grow the heap on init
        if (!grow_heap(requested))
        {
            log_msg("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n", 
                size, (void*)NULL, (size_t)0);
            return NULL;
        }
    }
    // Put the header onto heap
    header_t *new_h = find_fit(requested);
    if (!new_h)
    {
        // Grow heap if needed
        if (!grow_heap(requested))
        {
            log_msg("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n", 
                size, (void*)NULL, (size_t)0);
            return NULL;
        }
        new_h = find_fit(requested);
        // If fails, then we are out of memory!
        if (!new_h)
        {
            log_msg("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n", 
                size, (void*)NULL, (size_t)0);
            return NULL;
        }
    }
    // Split the block if needed
    split_block(new_h, requested);
    new_h->is_used = true;
    void *p = PAYLOAD_FROM_HDR(new_h);

    log_msg("MALLOC: malloc(%zu) => (ptr=%p, size=%zu)\n", 
            size, p, new_h->size);
    // Return payload pointer
    return p;
}

/**
 * @brief free()!
 * @param ptr 
 */
void free(void* ptr)
{
    // Don't do anything if NULL
    if (ptr == NULL)
    {
        log_msg("MALLOC: free(%p)\n", (void*)NULL);
        return;
    }
    // If the ptr is not alligned, do nothing
    if((uintptr_t)ptr % ALIGNMENT != 0)
    {
        return;
    }

    header_t *h = HDR_FROM_PAYLOAD(ptr);
    
    // Check if header is in the list
    bool header_in_list = false;
    header_t *curr;
    for (curr = heap_head; curr; curr = curr->next)
    {
        if(curr == h)
        {
            header_in_list = true;
        }
    }
    if (header_in_list == false)
    {
        return;
    }

    // Ensure that the payload isn't being double freed
    if(!h->is_used)
    {
        log_msg("MALLOC: free(%p)\n", ptr);

        return;
    }

    h->is_used = false;
    log_msg("MALLOC: free(%p)\n", ptr);
}

/**
 * @brief 
 * 
 * calloc() takes two arguments: 
 * nmemb (number of elements)
 * size (bytes per element)
 * 
 *  Every byte in the returned block is set to 0
 * 
 * @param nmemb 
 * @param size 
 * @return void* 
 */
void *calloc(size_t nmemb, size_t size)
{
    // Ensure neither number of elements or size == 0
    if (nmemb == 0 || size == 0) {
        log_msg("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)\n",
            nmemb, size, (void*)NULL, (size_t)0);
        return NULL;
    }

    // Overflow check: nmemb * size must not wrap
    if (size > SIZE_MAX / nmemb) {
        log_msg("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)  // overflow\n",
            nmemb, size, (void*)NULL, (size_t)0);
        return NULL;
    }

    size_t total = nmemb * size;

    void *p = malloc(total);
    if (!p)
    {
        log_msg("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)\n",
                nmemb, size, (void*)NULL, (size_t)0);
        return NULL;
    }

    // Zero the allocated payload
    memset(p, 0, total);
    log_msg("MALLOC: calloc(%zu,%zu) => (ptr=%p, size=%zu)\n",
            nmemb, size, p, total);
    return p;
}

/**
 * @brief realloc()
 * 
 * @param ptr 
 * @param size 
 * @return void* 
 */
void *realloc(void* ptr, size_t size)
{
    // Acts as malloc() if NULL
    if (ptr == NULL)
    {
        void *p = malloc(size);
        log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
                (void *)NULL, size, p, p ? ALIGN(size) : (size_t)0);
        return p;
    }
    // Acts as free() if size == 0
    if (size == 0)
    {
        log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
                ptr, (size_t)0, (void*)NULL, (size_t)0);
        free(ptr);
        return NULL;
    }

    header_t *header = HDR_FROM_PAYLOAD(ptr);
    size_t requested = ALIGN(size);

    // Shrink in place
    if (header->size >= requested) 
    {
        // We can use this chunk for the new memory
        split_block(header, requested);
        log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
                ptr, size, ptr, header->size);
        return ptr;
    }
    else
    {
        // Grow in place
        header_t *next = header->next;

        // Ensure enough size
        if (next && !next->is_used &&
            ((header->size + HDR_SIZE + next->size) >= (requested)))
        {
            // Update current header and heap pointers
            header->size += HDR_SIZE + next->size;
            header->next = next->next;

            if (heap_tail == next)
                heap_tail = header;

            split_block(header, requested);
            return ptr;
        }
    }

    // If neither of those, then memcpy()

    header_t *new_h = find_fit(requested);
    if (!new_h)
    {
        // Grow heap if needed
        if (!grow_heap(requested))
        {
            log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
                    ptr, size, (void*)NULL, (size_t)0);
            return NULL;
        }
        new_h = find_fit(requested);

        if (!new_h)
        {
            log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
                    ptr, size, (void*)NULL, (size_t)0);
            return NULL;
        }
    }

    split_block(new_h, requested);
    new_h->is_used = true;

    // Get the new payload pointer and copy over 
    void *newp = PAYLOAD_FROM_HDR(new_h);
    size_t to_copy = header->size < requested ? header->size : requested;

    memcpy(newp, ptr, to_copy);

    log_msg("MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",
            ptr, size, newp, requested);
    free(ptr);
    return newp;
}
