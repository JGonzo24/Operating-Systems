/* malloc.h — public interface for your custom allocator
 *
 * NOTE: This header declares standard malloc-family symbols so that
 * code including <stdlib.h> will still match types exactly. If you
 * also include the system <malloc.h>, ensure your include paths pick
 * this file first, or rename this header to avoid collisions.  */

#ifndef CUSTOM_MALLOC_H_
#define CUSTOM_MALLOC_H_

#include <stddef.h> /* size_t */
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>   // write
#include <stdio.h>    // snprintf
#include <stdlib.h>   // getenv
#include <string.h>   // strlen
#include <stdarg.h>   // va_list


#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct header
    {
        size_t size;
        bool is_used;
        struct header *next;
    } header_t;

    void insert_free_block(header_t *h);
    /* Allocate a block of at least `size` bytes, aligned to suitable boundary.
     * Returns NULL on failure or if size == 0 (by your current policy).
     */
    void *malloc(size_t size);

    /* Free a block previously returned by malloc/calloc/realloc.
     * Safe to call with NULL (no-op).
     */
    void free(void *ptr);

    /* Resize a previously allocated block.
     * - Shrinks in place when possible (may return same pointer).
     * - Expands in place by merging right-adjacent free space when possible.
     * - Otherwise allocates a new block, copies min(old,new) bytes,
     *   frees the old block, and returns the new pointer.
     * - If size == 0 and ptr != NULL, behaves like free(ptr) and returns NULL.
     */
    void *realloc(void *ptr, size_t size);

    /* Allocate an array of nmemb elements of `size` bytes each and zero-fill.
     * Returns NULL on overflow (nmemb * size) or allocation failure.
     * Your policy: returns NULL if nmemb == 0 or size == 0.
     */
    void *calloc(size_t nmemb, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CUSTOM_MALLOC_H_ */
