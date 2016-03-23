#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <libelf/libelf.h>
#include <libelf/gelf.h>
#include <pthread.h>


#define MAXSTACKSIZE 20
#define MIN(a,b) ((a) < (b) ? (a) :(b))

//Replaced functions
static void* (*real_calloc)(size_t,size_t) = NULL;
static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void *) = NULL;
static int (*real_execve)(const char *filename, char *const argv[],
        char *const envp[]) = NULL;
static size_t PAGE_SIZE=0;
static const char * OutFileName = "structs.csv";

// Main executable
static char *mainFile;
// Lock for naming allocations
static pthread_mutex_t lock;
static FILE * OutFile;
// 1 during init, 0 after, to avoid circular dependencies (malloc/dlsym)
static int initializing=1;

void __attribute__ ((constructor)) init(void);

void init(void){
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_free = dlsym(RTLD_NEXT, "free");
    real_execve = dlsym(RTLD_NEXT, "execve");
    pthread_mutex_init(&lock,NULL);
    PAGE_SIZE=getpagesize();
    /* Important note: the lib will be re initialized after each call to
     * execve*/
    if(!(OutFile=fopen(OutFileName, "r"))){
        // First open
        OutFile=fopen("structs.csv","w+");
        fprintf(OutFile,"name,addr,size,backtrace\n");
        fflush(OutFile);
    }else{
        fclose(OutFile);
        OutFile=fopen("structs.csv","a");
    }
    initializing=0;
}


/* =========================================================================
 *                          Helpers declaration
 * ========================================================================= */

// Handle allocation (retrieve backtrace and print it to output file
void handle_alloc(size_t sz, int *addr, char *prefix);
// Return a short call stack
char *ShortCallStack(char** calls, size_t size);
// Print an allocation to the output file
void print_struct(char *bt,char *name, unsigned long addr, size_t sz);
// Remove preload useful before explicitly calling execve
void remove_preload(void);
// Retrieve static structures from a binary file
int get_structs(const char* file);

/* =========================================================================
 *                          Wrappers
 * ========================================================================= */

/* Allocator for initialization */

#define TMPBUFFSZ 4*1024*1024 //1M
char tmpBuff[TMPBUFFSZ];
int tmppos=0;
void* initMalloc(size_t sz)
{
    char *ret=tmpBuff+tmppos;
    tmppos+=sz;
    return (void *)ret;
}

// Replace free to avoid freeing our temporary buffer
void free(void* ptr)
{
    if(!initializing && (unsigned long)ptr >= (unsigned long)(tmpBuff+TMPBUFFSZ))
        real_free(ptr);
}

// Replace malloc
void* malloc(size_t sz)
{
    static __thread int no_hook=0;


    if(initializing) // Avoid circular dependency between malloc and dlsym
        return initMalloc(sz);

    void *ret = real_malloc(sz);
    if(no_hook==0)
    {
        no_hook=1;
        handle_alloc(sz,ret, "malloc");
        no_hook=0;
    }
    return ret;
}


// Replace calloc
void* calloc(size_t nmb,size_t sz)
{
    static __thread int no_hook=0;
    void *ret=NULL;

    if(initializing){ // Avoid circular dependency between calloc and dlsym
        ret=initMalloc(nmb*sz);
        memset(ret,0,nmb*sz);
        return ret;
    }

    ret = real_calloc(nmb,sz);
    if(no_hook==0)
    {
        no_hook=1;
        handle_alloc(nmb*sz,ret, "calloc");
        no_hook=0;
    }
    return ret;
}

int execve(const char *filename, char *const argv[], char *const envp[]){
    int pid,st;
    mainFile=basename(filename);
    printf("Executing %s [%s]\n", filename, mainFile);
    char tempfilename[]="/tmp/replacemallocXXXXXX";
    int fd=mkstemp(tempfilename);
    char buff[300];
    // Open main output file
    /* List libraries, we need to do an exec here, so we fork and remove the
     * LD_PRELOAD from the child */
    if(!(pid=fork())){
        remove_preload();
        snprintf(buff,300,"/usr/bin/ldd %s | awk '{print $3}' > %s", filename, tempfilename);
        system(buff);
        close(fd);
        exit(0);
    }
    else{
        waitpid(pid, &st, 0);
    }
    // Retrieve main structures
    get_structs(filename);
    // Retrieve structures of shared libraries
    size_t read,len=0;
    char *line=0;
    FILE *fp=fdopen(fd,"r");
    while((read=getline(&line, &len, fp)) != -1)
        if(read>1){
            line[strlen(line)-1]='\0';// remove trailing '\n'
            get_structs(line);
        }
    fclose(fp);
    close(fd);
    remove(tempfilename);
    return real_execve(filename,argv,envp);
}

/* =========================================================================
 *                                  Helpers
 * ========================================================================= */

// Handle allocation (retrieve backtrace and print it to output file
void handle_alloc(size_t sz, int *addr, char *prefix)
{
    if(sz < PAGE_SIZE)
        return;
    void *array[MAXSTACKSIZE];
    size_t size;
    char **strings;
    size_t i=0;

    size = backtrace (array, MAXSTACKSIZE);
    strings = backtrace_symbols (array, size);
    char *newstring=ShortCallStack(strings,size);

    print_struct(newstring+1/*ignore starting '/'*/,prefix,(unsigned long)addr,sz);
    free (strings);
    free(newstring);
}

// Return a short call stack
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
                        break;
                    case '[':
                        // Next string starts right after the '['
                        startOffset=curOffset+1;
                        break;
                    case ']':
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

// Print an allocation to the output file
void print_struct(char *bt,char *name, unsigned long addr, size_t sz){
    static int allocid=0;
    pthread_mutex_lock(&lock);
    char buff[150];
    char *n;
    if(strcmp(bt,"")!=0){
        sprintf(buff,"%s#%d",name,allocid);
        n=buff;
        allocid++;
    }else{
        n=name;
    }
    fprintf(OutFile,"%s,%lu,%lu,%s\n",n,addr,sz,bt);
    fflush(OutFile);
    pthread_mutex_unlock(&lock);
}

extern char ** environ;
// Remove preload useful before explicitly calling execve
void remove_preload(void)
{
    char **env = environ;
    while(*env) {
        if(strstr(*env, "LD_PRELOAD=") == *env) {
            break;
        }
        env++;
    }
    while(*env) {
        *env = *(env + 1);
        env++;
    }
}


/*
 * The following function is an adaptation of the libelf-howto.c from:
 * http://em386.blogspot.com
 *
 */

#define ERR -1

int get_structs(const char* file)
{
    Elf *elf;                       /* Our Elf pointer for libelf */
    Elf_Scn *scn=NULL;                   /* Section Descriptor */
    Elf_Data *edata=NULL;                /* Data Descriptor */
    GElf_Sym sym;			/* Symbol */
    GElf_Dyn dyn;			/* Dynamic */
    GElf_Shdr shdr;                 /* Section Header */


    int fd; 		// File Descriptor
    char *base_ptr;		// ptr to our object in memory
    struct stat elf_stats;	// fstat struct

    if((fd = open(file, O_RDONLY)) == ERR)
    {
        fprintf(stderr,"Unable to open: '%s'\n", file);
        return ERR;
    }

    if((fstat(fd, &elf_stats)))
    {
        fprintf(stderr,"Unable to fstat: '%s'\n", file);
        close(fd);
        return ERR;
    }

    if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
    {
        fprintf(stderr,"Unable to malloc: '%lu'\n", elf_stats.st_size);
        close(fd);
        return ERR;
    }

    if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
    {
        fprintf(stderr,"Unable to read fstat: '%s'\n", file);
        free(base_ptr);
        close(fd);
        return ERR;
    }

    /* Check libelf version first */
    if(elf_version(EV_CURRENT) == EV_NONE)
    {
        fprintf(stderr,"Elf library is out of date\n");
        close(fd);
        return ERR;
    }

    elf = elf_begin(fd, ELF_C_READ, NULL);	// Initialize 'elf' pointer to our file descriptor

    elf = elf_begin(fd, ELF_C_READ, NULL);

    int symbol_count;
    int i;

    while((scn = elf_nextscn(elf, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        // Get the symbol table
        if(shdr.sh_type == SHT_SYMTAB)
        {
            // edata points to our symbol table
            edata = elf_getdata(scn, edata);
            // how many symbols are there? this number comes from the size of
            // the section divided by the entry size
            symbol_count = shdr.sh_size / shdr.sh_entsize;
            // loop through to grab all symbols
            for(i = 0; i < symbol_count; i++)
            {
                // libelf grabs the symbol data using gelf_getsym()
                gelf_getsym(edata, i, &sym);
                // Keep only objects big enough to be data structures
                if(ELF32_ST_TYPE(sym.st_info)==STT_OBJECT &&
                        sym.st_size >= PAGE_SIZE)
                {
                    print_struct("NA",elf_strptr(elf, shdr.sh_link, sym.st_name),
                            sym.st_value, sym.st_size);
                }
            }
        }
    }
    return 0;
}
