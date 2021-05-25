#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gcmalloc.h"

static size_t chunk_size;

static void iterate(void *ptr, size_t size, void *data)
{
    (*((size_t *) data))++;
}

static void check_chunks(size_t expect_used, size_t expect_avail)
{
    size_t act_used = 0;
    size_t act_avail = 0;

    gc_iterate_avail(iterate, &act_avail);
    gc_iterate_used(iterate, &act_used);
    if (expect_used != act_used)
    {
        printf("expect used: %zu, actual used: %zu\n", expect_used, act_used);
        abort();
    }
    if (expect_avail != act_avail)
    {
        printf("expect avail: %zu, actual avail: %zu\n", expect_avail, act_avail);
        abort();
    }
}

static void test_malloc(void)
{
    void *data;
    void *data1;
    void *data2;

    gc_malloc(0);
    gc_free(NULL);
    check_chunks(0, 0);
    data = gc_malloc(chunk_size * 8);
    check_chunks(1, 0);
    gc_free(data);
    check_chunks(0, 1);

    // Allocate and free 3 chunks
    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data);
    check_chunks(2, 1);
    gc_free(data1);
    check_chunks(1, 1);
    gc_free(data2);
    check_chunks(0, 1);

    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data);
    check_chunks(2, 1);
    gc_free(data2);
    check_chunks(1, 2);
    gc_free(data1);
    check_chunks(0, 1);

    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data1);
    check_chunks(2, 1);
    gc_free(data);
    check_chunks(1, 1);
    gc_free(data2);
    check_chunks(0, 1);

    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data1);
    check_chunks(2, 1);
    gc_free(data2);
    check_chunks(1, 1);
    gc_free(data);
    check_chunks(0, 1);

    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data2);
    check_chunks(2, 1);
    gc_free(data);
    check_chunks(1, 2);
    gc_free(data1);
    check_chunks(0, 1);

    data = gc_malloc(chunk_size * 2);
    data1 = gc_malloc(chunk_size * 2);
    data2 = gc_malloc(chunk_size * 2);
    check_chunks(3, 0);
    gc_free(data2);
    check_chunks(2, 1);
    gc_free(data1);
    check_chunks(1, 1);
    gc_free(data);
    check_chunks(0, 1);

    // Don't leave a single remaining chunk
    data = gc_malloc(chunk_size * 7);
    check_chunks(1, 0);
    gc_free(data);
    check_chunks(0, 1);
}

static void test_calloc(void)
{
    char expected[chunk_size * 8];
    void *actual;

    memset(expected, 0, chunk_size * 8);
    actual = gc_calloc(1, chunk_size * 8);
    if (memcmp(actual, expected, chunk_size * 8) != 0)
    {
        printf("expected bytes not zeroed\n");
        abort();
    }
    memset(actual, 1, chunk_size * 8);
    gc_free(actual);
    actual = gc_calloc(1, chunk_size * 8);
    if (memcmp(actual, expected, chunk_size * 8) != 0)
    {
        printf("expected bytes not zeroed\n");
        abort();
    }
    gc_free(actual);
}

static void test_realloc(void)
{
    char expected[chunk_size * 8];
    void *actual;

    memset(expected, 1, chunk_size * 8);
    actual = gc_realloc(NULL, chunk_size * 8);
    gc_realloc(actual, 0);
    check_chunks(0, 1);
    actual = gc_realloc(NULL, chunk_size * 8);
    memset(actual, 1, chunk_size * 8);
    // Same.
    actual = gc_realloc(actual, chunk_size * 8);
    if (memcmp(actual, expected, chunk_size * 8) != 0)
    {
        printf("expected bytes different\n");
        abort();
    }
    // Larger
    actual = gc_realloc(actual, chunk_size * 8 * 2);
    if (memcmp(actual, expected, chunk_size * 8) != 0)
    {
        printf("expected bytes different\n");
        abort();
    }
    // Smaller
    actual = gc_realloc(actual, chunk_size);
    if (memcmp(actual, expected, chunk_size) != 0)
    {
        printf("expected bytes different\n");
        abort();
    }
    gc_free(actual);
    check_chunks(0, 1);
}

void *i = (void *) 0x1;
void *j;
static void *k = (void *) 0x1;
static void *l;


static void test_data(void)
{
    gc_collect();
    check_chunks(0, 1);
    i = gc_malloc(1);
    k = gc_malloc(1);
    gc_collect();
    check_chunks(2, 1);
    i = NULL;
    k = NULL;
    gc_collect();
    check_chunks(0, 1);
}

static void test_bss(void)
{
    check_chunks(0, 1);
    j = gc_malloc(1);
    l = gc_malloc(1);
    gc_collect();
    check_chunks(2, 1);
    j = NULL;
    l = NULL;
    gc_collect();
    check_chunks(0, 1);
}

static void push(void)
{
    void *data;

    data = gc_malloc(1);
    (void) data;
    gc_collect();
    check_chunks(2, 1);
}

static void test_stack(void)
{
    void *data;
    
    check_chunks(0, 1);
    data = gc_malloc(1);
    (void) data;
    push();
    gc_collect();
    data = NULL;
    check_chunks(1, 1);
    gc_collect();
    check_chunks(0, 1);
}

static void test_heap(void)
{
    struct data
    {
        void *ptr;
    } *data;

    check_chunks(0, 1);
    data = gc_malloc(sizeof data);
    data->ptr = gc_malloc(1);
    gc_collect();
    check_chunks(2, 1);
    data = NULL;
    gc_collect();
    check_chunks(1, 2);
    gc_collect();
    check_chunks(0, 1);
}

static void test_collect(void)
{
    test_data();
    test_bss();
    test_stack();
    test_heap();
}

int main(void)
{
    chunk_size = gc_chunk_size();
    test_malloc();
    test_calloc();
    test_realloc();
    test_collect();
    return EXIT_SUCCESS;
}
