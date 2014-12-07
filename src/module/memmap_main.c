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
// Maximum number of process (pertinent only if the user set the module param)
int MemMap_maxProcess=0;
// Wakeup period in ms (defined in memmap_tlb.c)
extern int MemMap_wakeupInterval;

module_param(MemMap_mainPid, int, 0);
module_param(MemMap_wakeupInterval,int,0);
module_param(MemMap_schedulerPriority,int,0);
module_param(MemMap_maxProcess,int,0);

//Number of threads (one per CPU)
int MemMap_numThreads=1;

void MemMap_CleanUp(void)
{
    printk(KERN_WARNING "MemMap Unregistering probes\n");
    MemMap_UnregisterProbes();
    //Kill all threads
    printk(KERN_WARNING "MemMap Killing threads\n");
    MemMap_CleanThreads();
    //Clean memory
    printk(KERN_WARNING "MemMap Removing shared data\n");
    MemMap_CleanProcessData();
}


// Fuction called by insmod
static int __init MemMap_Init(void)
{
    printk(KERN_WARNING "MemMap started monitoring pid %d\n",
            MemMap_mainPid);
    MemMap_numThreads=num_online_cpus();
    //Remove previous MemMap entries
    if(MemMap_InitProcessManagment(MemMap_maxProcess,MemMap_mainPid)!=0)
        return -1;
    printk(KERN_WARNING "MemMap common data ready \n");
    if(MemMap_InitThreads()!=0)
        return -2;
    printk(KERN_WARNING "MemMap threads ready \n");
    if(MemMap_RegisterProbes()!=0)
        return -3;
    printk(KERN_WARNING "MemMap probes ready \n");
    printk(KERN_WARNING "MemMap correctly intialized \n");
    //Send signal to son process
    return 0;
}

// function called by rmmod
static void __exit MemMap_Exit(void)
{
    printk(KERN_WARNING "MemMap exiting\n");
    MemMap_CleanUp();
    printk(KERN_WARNING "MemMap exited\n");
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
