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
//#define MEMMAP_DEBUG

#include <linux/init.h>
#include <linux/kthread.h>
#include <asm-generic/atomic-long.h>
#include "memmap.h"
#include "memmap_page.h"
#include "memmap_probes.h"
#include "memmap_tasks.h"

#define MEMMAP_DEFAULT_SCHED_PRIO 99

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
// Priority for FIFO scheduler
int MemMap_schedulerPriority=MEMMAP_DEFAULT_SCHED_PRIO;

module_param(MemMap_mainPid, int, 0);
module_param(MemMap_wakeupInterval,int,0);
module_param(MemMap_schedulerPriority,int,0);
module_param(MemMap_taskDataHashBits,int,0);
module_param(MemMap_taskDataTableFactor,int,0);
module_param(MemMap_nbChunks,int,0);

// Thread task representation
struct task_struct *MemMap_threadTask=NULL;

// Vector clock
atomic_long_t MemMap_threadClock=ATOMIC_LONG_INIT(0);
void MemMap_UpdateClock(void)
{
    atomic_long_inc(&MemMap_threadClock);
}
unsigned long MemMap_GetClock(void)
{
    unsigned long ret=atomic_long_read(&MemMap_threadClock);
    return ret;
}

// Initializes threads data structures
int MemMap_InitThreads(void)
{
    struct sched_param param;
    param.sched_priority=MemMap_schedulerPriority;
    //Creating the thread
    MemMap_threadTask=kthread_create(MemMap_MonitorThread, NULL,
            "MemMap tlb walker thread");
    MEMMAP_DEBUG_PRINT("MemMap kthread task %p\n", MemMap_threadTask);
    if(!MemMap_threadTask)
    {
        MemMap_Panic("MemMap Kthread create failed");
        return -1;
    }
    get_task_struct(MemMap_threadTask);
    // Set priority
    sched_setscheduler(MemMap_threadTask,SCHED_FIFO,&param);
    //And finally start it
    wake_up_process(MemMap_threadTask);
    return 0;
}

void MemMap_CleanUp(void)
{
    MEMMAP_DEBUG_PRINT("MemMap Unregistering probes\n");
    MemMap_UnregisterProbes();
    if(MemMap_threadTask && current != MemMap_threadTask)
    {
        MEMMAP_DEBUG_PRINT("Killing thread task %p\n",MemMap_threadTask);
        kthread_stop(MemMap_threadTask);
        put_task_struct(MemMap_threadTask);
    }
    //Clean memory
    MEMMAP_DEBUG_PRINT("MemMap Removing shared data\n");
    MemMap_CleanProcessData();
}


// Fuction called by insmod
static int __init MemMap_Init(void)
{
    printk(KERN_NOTICE "MemMap started monitoring pid %d\n",
            MemMap_mainPid);
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

module_init(MemMap_Init);
module_exit(MemMap_Exit);
