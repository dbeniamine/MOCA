#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>

#define MAXSTACKSIZE 20
#define MIN(a,b) ((a) < (b) ? (a) :(b))

char *ShortCallStack(char** calls, size_t size)
{

    int i,ended=0;
    size_t startOffset,curOffset,curSize; //Offsets of the current read string
    size_t prevOffset=0,prevSize=0;// Previous file name in string
    size_t curEnd=0; // Last character in ret
    char *ret=calloc(2*strlen(calls[0])*size,1); //should be enough

    for(i=0;i<size;++i){
        // Ignore calls in this library
        if( strncmp(calls[i],__LIBNAME__,strlen(__LIBNAME__))!=0){
            // Generate the concatenated string
            curOffset=startOffset=ended=0;
            while(!ended && curOffset < strlen(calls[i])){
                switch(calls[i][curOffset]){
                    case '/':
                        // Next dir in lib path
                        startOffset=curOffset;
                        break;
                    case '(':
                        // End of lib name
                        curSize=curOffset-startOffset;
                        if(prevSize==0 || strncmp(ret+prevOffset,calls[i]+startOffset,
                                    MIN(prevSize,curSize))!=0){
                            // Current name need to be added in the string
                            strncpy(ret+curEnd,calls[i]+startOffset,curSize);
                            // Update offset to the lib name
                            prevOffset=curEnd;
                            prevSize=curSize;
                            curEnd+=curSize;
                        }
                        // Next string starts right after the '('
                        startOffset=curOffset+1;
                        break;
                    case ')':
                        // End of fct name / offset
                        curSize=curOffset-startOffset;
                        ret[curEnd++]=':';
                        strncpy(ret+curEnd,calls[i]+startOffset,curSize);
                        curEnd+=curSize;
                        ended=1;
                        break;
                    default:
                            break;
                }
                ++curOffset;
            }
        }
    }
    ret[curEnd]='\0';
    return ret;
}

// Retrieve stack trace using backtrace
// Print out alloc
void handle_alloc(size_t sz, int *addr)
{
    static size_t pgsize=0;
    if(!pgsize)
        pgsize=getpagesize();
    if(sz < pgsize)
        return;
    void *array[MAXSTACKSIZE];
    size_t size;
    char **strings;
    size_t i=0;

    size = backtrace (array, MAXSTACKSIZE);
    strings = backtrace_symbols (array, size);
    char *newstring=ShortCallStack(strings,size);
    printf("Moca malloc detected: %lu,%lu,%s\n",(unsigned long)addr,sz,
            newstring+1/*ignore starting '/'*/);
    free (strings);
    free(newstring);
}

char tmpBuff[4096];
int tmppos=0;
void* initMalloc(size_t sz)
{
    char *ret=tmpBuff+tmppos;
    tmppos+=sz;
    return (void *)ret;
}


// Replace malloc
void* malloc(size_t sz)
{
    static __thread int no_hook=0;
    static void* (*real_malloc)(size_t) = NULL;


    if (!real_malloc)
    {
        if(no_hook) // Avoid circular dependency between malloc and dlsym
            return initMalloc(sz);
        no_hook=1;
        real_malloc = dlsym(RTLD_NEXT, "malloc");
        no_hook=0;
    }


        real_malloc = dlsym(RTLD_NEXT, "malloc");

    void *ret = real_malloc(sz);
    if(no_hook==0)
    {
        no_hook=1;
        handle_alloc(sz,ret);
        no_hook=0;
    }
    return ret;
}


// Replace calloc
void* calloc(size_t nmb,size_t sz)
{
    static __thread int no_hook=0;
    static void* (*real_calloc)(size_t,size_t) = NULL;
    void *ret=NULL;

    if (!real_calloc)
    {
        if(no_hook) // Avoid circular dependency between calloc and dlsym
        {
            ret=initMalloc(nmb*sz);
            memset(ret,0,nmb*sz);
            return ret;
        }
        no_hook=1;
        real_calloc = dlsym(RTLD_NEXT, "calloc");
        no_hook=0;
    }

    ret = real_calloc(nmb,sz);
    if(no_hook==0)
    {
        no_hook=1;
        handle_alloc(nmb*sz,ret);
        no_hook=0;
    }
    return ret;
}
