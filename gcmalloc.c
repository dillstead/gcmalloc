#define _BSD_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "debug_log.h"
#include "gcmalloc.h"

#define ESP 0
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
// Used to ensure proper alignment of allocated memory.
union align
{
    int i;
    long l;
    long *lp;
    void *p;
    void (*fp)(void);
    float f;
    double d;
    long double ld;
};

struct header
{
    size_t size; 
    union chunk *next;
};

union chunk
{
    struct header h;
    union align a;
};

static union chunk *avail;
static union chunk *used;

// Start of initialized data segment.
extern void *__data_start;
// End of the intialize data segment and start of the
// uninitialized data segment.
extern void *_edata;
// End of the uninitialized data segment.
extern void *_end;
// Storage for the following callee saved registers:
//   rsp, rbx, rbp, r12 - r15
// The registers will be stored in the bss segment
// and, if they contain the target value, they will
// get picked up in the bss scan.
static void *regs[7];
extern void get_regs(void *regs);

static void *stack_bot(void)
{
    FILE *fp;
    unsigned long bot;

    fp = fopen("/proc/self/stat", "r");
    if (fp == NULL)
    {
        return NULL;
    }
    fscanf(fp,"%*d %*s %*c%*d%*d%*d%*d%*d%*u%*lu%*lu%*lu%*lu"
           "%*lu%*lu%*ld%*ld%*ld%*ld%*ld%*ld%*llu%*lu%*ld%*lu"
           "%*lu%*lu%lu", &bot);
    fclose(fp);
    return (void *) bot;
}

static union chunk **insert(union chunk **pnext, union chunk *chunk)
{
    union chunk **prev = NULL;
    union chunk *cur = *pnext;
    
    while (cur && chunk > cur)
    {
        prev = pnext;
        pnext = &cur->h.next;
        cur = cur->h.next;
    }
    *pnext = chunk;
    chunk->h.next = cur;

    return prev;
}

static union chunk **merge(union chunk **pnext, union chunk *chunk)
{
    union chunk **prev;
    
    prev = insert(pnext, chunk);
    if (chunk->h.next && ((char *) chunk + chunk->h.size) == (char *) chunk->h.next)
    {
        // Merge with the next chunk.
        chunk->h.size += chunk->h.next->h.size;
        chunk->h.next = chunk->h.next->h.next;
    }
    if (prev && (((char *) (*prev)) + (*prev)->h.size) == (char *) chunk)
    {
        // Merge with the previous chunk.
        (*prev)->h.next = chunk->h.next;
        (*prev)->h.size += chunk->h.size;
    }
    if (!prev)
    {
        prev = pnext;
    }
    return prev;
}

static void iterate(union chunk *cur, iterate_func *cb, void *data)
{
    while (cur)
    {
        cb(cur, cur->h.size, data);
        cur = cur->h.next;
    }
}

static bool mem_scan(const char *tag, void *target, void *begin, void *end)
{
    for (void **i = (void **) begin; i < (void **) end; i++)
    {
        if (*i == target)
        {
            LOG("Found target %p at %p in %s", target, (void *) i, tag);
            return true;
        }
    }
    return false;
}

static bool heap_scan(void *target)
{
    union chunk *cur = used;

    while (cur)
    {
        if (mem_scan("heap", target, cur + 1, ((char *) cur + 1) + cur->h.size))
        {
            return true;
        }
        cur = cur->h.next;
    }
    return false;
}

static void *_malloc(size_t size)
{
    union chunk *cur = avail;
    union chunk **pnext = &avail;
    union chunk *next;

    if (!size)
    {
        return NULL;
    }
    size = (size + sizeof(union chunk) - 1) / sizeof(union chunk)
        * sizeof(union chunk);
    // Add 1 chunk for the header.
    size += sizeof(union chunk);
    while (cur && size  > cur->h.size)
    {
        pnext = &cur->h.next;
        cur = cur->h.next;
    }
    if (cur)
    {
        if (cur->h.size - size < 2 * sizeof(union chunk))
        {
            // Not enough space left for header + extra space
            // so use the entire chunk.
            *pnext = cur->h.next;            
        }
        else
        {
            // Split the chunk.
            next = (union chunk *) ((char *) cur + size);
            *pnext = next;
            next->h.size = cur->h.size - size;
            next->h.next = cur->h.next;
            cur->h.size = size;
        }
    }
    else
    { 
        cur = sbrk(size);
        if (cur == (void *) -1)
        {
            return NULL;
        }
        cur->h.size = size;
    }
    insert(&used, cur);
    return cur + 1;
}

static void _free(void *ptr)
{
    union chunk *cur = used;
    union chunk **pnext = &used;
    
    if (!ptr)
    {
        return;
    }
    ptr = (union chunk *) ptr - 1;
    while (cur && cur != ptr)
    {
        pnext = &cur->h.next;
        cur = cur->h.next;
    }
    *pnext = cur->h.next;
    merge(&avail, cur);
}

void *gc_malloc(size_t size)
{
    void *ptr;
    
    ptr = _malloc(size);
    LOG("gc_malloc size: %zu, %p", size, ptr);    
    return ptr;
}

void gc_free(void *ptr)
{
    LOG("gc_free %p", ptr);
    _free(ptr);
}

void *gc_calloc(size_t nmemb, size_t size)
{
    void *ptr;

    ptr = _malloc(nmemb * size);
    if (ptr)
    {
        memset(ptr, 0, nmemb * size);        
    }
    LOG("gc_calloc memb: %zu, size: %zu, %p", nmemb, size, ptr);
    return ptr;
}

void *gc_realloc(void *ptr, size_t size)
{
    void *newptr = NULL;
    union chunk *chunk = (union chunk *) ptr - 1;

    if (size)
    {
        newptr = _malloc(size);
        if (ptr)
        {
            memcpy(newptr, ptr, MIN(chunk->h.size, size));
        }
    }
    _free(ptr);
    LOG("gc_realloc ptr: %p, size: %zu, %p", ptr, size, newptr);
    return newptr;
}

void gc_collect(void)
{
    static void *bot;
    union chunk *cur = used;
    union chunk **pnext = &used;
    union chunk **panext = &avail;

    LOG("gc_collect");
    get_regs(regs);
    if (!bot)
    {
        // The stack bottom should not change.
        bot = stack_bot();
    }
    // Scan the memory and heap for references to each chunk.
    while (cur)
    {
        cur->h.size |= mem_scan("stack", cur + 1, regs[ESP], bot)
            || mem_scan("data", cur + 1, &__data_start, &_edata)
            || mem_scan("bss", cur + 1, &_edata, &_end)
            || heap_scan(cur + 1);
        cur = cur->h.next;
    }
    // Remove unused chunks and add them to the list of available
    // chunks.
    cur = used;
    while (cur)
    {
        if (cur->h.size & 0x1)
        {
            cur->h.size &= ~1;
            pnext = &cur->h.next;
            cur = cur->h.next;
            continue;
        }
        *pnext = cur->h.next;
        panext = merge(panext, cur);
        cur = *pnext;
    }
}

size_t gc_chunk_size(void)
{
    return sizeof (union chunk);
}

void gc_iterate_used(iterate_func *cb, void *data)
{
    iterate(used, cb, data);
}

void gc_iterate_avail(iterate_func *cb, void *data)
{
    iterate(avail, cb, data);
}
