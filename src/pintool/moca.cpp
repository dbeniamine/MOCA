/*
 * Copyright (C) 2015  Diener, Matthias <mdiener@inf.ufrgs.br> and Beniamine, David <David@Beniamine.net>
 * Author: Beniamine, David <David@Beniamine.net>
 * Author: Diener, Matthias <mdiener@inf.ufrgs.br>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <tuple>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <numa.h>
#include <numaif.h>

#include <libelf/libelf.h>
#include <libelf/gelf.h>
#include <execinfo.h>


#include "pin.H"

#define REAL_TID(tid) ((tid)>=2 ? (tid)-1 : (tid))
#define ACC_T_READ 0
#define ACC_T_WRITE 1

char TYPE_NAME[2]={'R','W'};

const int MAXTHREADS = 1024;
int PAGESIZE;
unsigned int REAL_PAGESIZE;
KNOB<string>output_path(KNOB_MODE_WRITEONCE,"pintool", "o", "", "Output in directory (default \"\")");




struct{
    string sym;
    ADDRINT sz;
    int ended; // 0 if the malloc is not completed (missing ret val)
} Allocs[MAXTHREADS+1];

ofstream fstructStream;

int num_threads = 0;



string img_name;


long GetStackSize()
{
    struct rlimit sl;
    int returnVal = getrlimit(RLIMIT_STACK, &sl);
    if (returnVal == -1)
    {
        cerr << "Error. errno: " << errno << endl;
    }
    return sl.rlim_cur;
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    __sync_add_and_fetch(&num_threads, 1);

    if (num_threads>=MAXTHREADS+1) {
        cerr << "ERROR: num_threads (" << num_threads << ") higher than MAXTHREADS (" << MAXTHREADS << ")." << endl;
    }

}


//retrieve structures names address and size
int getStructs(const char* file);
/* string get_struct_name(string str, int ln, string fname, int rec); */


VOID PREMALLOC(ADDRINT retip, THREADID tid, ADDRINT sz, const string *binSource)
{
    int col, ln;
    int id=REAL_TID(tid);
    static int anonid=0;
    /* static int warned=0; */
    string fname;
    if( (unsigned int) sz >= REAL_PAGESIZE)
    {

        PIN_LockClient();
        PIN_GetSourceLocation 	(retip, &col, &ln, &fname);
        PIN_UnlockClient();
	Allocs[id].sym=*binSource+string(":");
        if(fname.compare("")!=0){
            Allocs[id].sym+=fname+string(":") + to_string(ln);
        }else
        {
            Allocs[id].sym+="UnnamedStruct#" + to_string(anonid++);
        }
        Allocs[id].sz=sz;
        Allocs[id].ended=0;
    }
}
VOID POSTMALLOC(ADDRINT ret, THREADID tid)
{
    int id=REAL_TID(tid);
    //static int anonymousId=0;
    if (Allocs[id].ended==0)
    {
        fstructStream  << Allocs[id].sym <<","<<ret<<","<<Allocs[id].sz<<endl;
        Allocs[id].ended=1;
    }
}

VOID binName(IMG img, VOID *v)
{
    string* name=new string(IMG_Name(img));
    string base=basename(name->c_str());
    if (IMG_IsMainExecutable(img))
    {
	cout <<output_path.Value()+string("/")+base+string(".structs.csv") << endl;
        fstructStream.open(output_path.Value()+string("/")+base+string(".structs.csv"));
        fstructStream << "name,start,sz" << endl;

    }
    getStructs(name->c_str());
    RTN mallocRtn = RTN_FindByName(img, "malloc");
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)PREMALLOC,
                IARG_RETURN_IP, IARG_THREAD_ID,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,IARG_PTR, name,  IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)POSTMALLOC,
                IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID,  IARG_END);
        RTN_Close(mallocRtn);
    }
}



VOID Fini(INT32 code, VOID *v)
{
    fstructStream.close();
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc,argv)) return 1;

    REAL_PAGESIZE=sysconf(_SC_PAGESIZE);
    PAGESIZE = log2(REAL_PAGESIZE);


    IMG_AddInstrumentFunction(binName, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
}

/*
 * The following function is an adaptation of the libelf-howto.c from:
 * http://em386.blogspot.com
 *
 */

#define ERR -1

int getStructs(const char* file)
{
    Elf *elf;                       /* Our Elf pointer for libelf */
    Elf_Scn *scn=NULL;                   /* Section Descriptor */
    Elf_Data *edata=NULL;                /* Data Descriptor */
    GElf_Sym sym;			/* Symbol */
    GElf_Shdr shdr;                 /* Section Header */




    int fd; 		// File Descriptor
    char *base_ptr;		// ptr to our object in memory
    struct stat elf_stats;	// fstat struct
    cout << "Retrieving data structures from file "<< file << endl;

    if((fd = open(file, O_RDONLY)) == ERR)
    {
        cerr << "couldnt open" << file << endl;
        return ERR;
    }

    if((fstat(fd, &elf_stats)))
    {
        cerr << "could not fstat" << file << endl;
        close(fd);
        return ERR;
    }

    if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
    {
        cerr << "could not malloc" << endl;
        close(fd);
        return ERR;
    }

    if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
    {
        cerr << "could not read" << file << endl;
        free(base_ptr);
        close(fd);
        return ERR;
    }

    /* Check libelf version first */
    if(elf_version(EV_CURRENT) == EV_NONE)
    {
        cerr << "WARNING Elf Library is out of date!" << endl;
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
                        sym.st_size >= REAL_PAGESIZE)
                {
                    fstructStream << elf_strptr(elf, shdr.sh_link, sym.st_name) <<
                        "," << sym.st_value << "," << sym.st_size << endl;
                }
            }
        }
    }
    return 0;
}
