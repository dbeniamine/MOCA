/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * MemMap is a kernel module designed to track memory access
 *
 * Copyright (C) 2010 David Beniamine
 * Author: David Beniamine <David.Beniamine@imag.fr>
 */

#include <linux/init.h>
#include "memmap.h"
#include "memmap_page.h"
#include "memmap_threads.h"
#include "memmap_probes.h"
#include "memmap_tasks.h"


/* Informations about the module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Beniamine David.Beniamine@imag.fr");
MODULE_DESCRIPTION("MemMap's kernel module tracks memory access");

/* Parameters */

// PID of the main program to track
int MemMap_mainPid=0;
// Wakeup period in ms (defined in memmap_tlb.c)
extern int MemMap_wakeupInterval;
extern int MemMap_taskDataHashBits;
extern int MemMap_taskDataTableFactor;
extern int MemMap_nbChunks;

module_param(MemMap_mainPid, int, 0);
module_param(MemMap_wakeupInterval,int,0);
module_param(MemMap_schedulerPriority,int,0);
module_param(MemMap_taskDataHashBits,int,0);
module_param(MemMap_taskDataTableFactor,int,0);
module_param(MemMap_nbChunks,int,0);

//Number of threads (one per CPU)
int MemMap_numThreads=1;

void MemMap_CleanUp(void)
{
    MEMMAP_DEBUG_PRINT("MemMap Unregistering probes\n");
    MemMap_UnregisterProbes();
    //Kill all threads
    MEMMAP_DEBUG_PRINT("MemMap Killing threads\n");
    MemMap_StopThreads();
    //Clean memory
    MEMMAP_DEBUG_PRINT("MemMap Removing shared data\n");
    MemMap_CleanProcessData();
    MemMap_CleanThreads();
}


// Fuction called by insmod
static int __init MemMap_Init(void)
{
    printk(KERN_NOTICE "MemMap started monitoring pid %d\n",
            MemMap_mainPid);
    MemMap_numThreads=num_online_cpus();
    //Remove previous MemMap entries
    if(MemMap_InitProcessManagment(MemMap_mainPid)!=0)
        return -1;
    MEMMAP_DEBUG_PRINT("MemMap common data ready \n");
    if(MemMap_InitThreads()!=0)
        return -2;
    MEMMAP_DEBUG_PRINT("MemMap threads ready \n");
    if(MemMap_RegisterProbes()!=0)
        return -3;
    MEMMAP_DEBUG_PRINT("MemMap probes ready \n");
    printk(KERN_NOTICE "MemMap correctly intialized \n");
    //Send signal to son process
    return 0;
}

// function called by rmmod
static void __exit MemMap_Exit(void)
{
    printk(KERN_NOTICE "MemMap exiting\n");
    MemMap_CleanUp();
    printk(KERN_NOTICE "MemMap exited\n");
}

// Panic exit function
void MemMap_Panic(const char *s)
{
    printk(KERN_ALERT "MemMap panic:\n%s\n", s);
    MemMap_CleanUp();
}

int MemMap_NumThreads(void)
{
    return MemMap_numThreads;
}

module_init(MemMap_Init);
module_exit(MemMap_Exit);
