#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#define LIM 0X7f0000000000

void* malloc(size_t sz)
{
    static void* (*real_malloc)(size_t) = NULL;
    if (!real_malloc)
        real_malloc = dlsym(RTLD_NEXT, "malloc");

    void *ret = real_malloc(sz);
    if((unsigned long) ret >= LIM)
        printf("Moca high malloc detected at %lu\n",(unsigned long)ret);
    return ret;
}
