/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Moca is a kernel module designed to track memory access
 *
 * Copyright (C) 2010 David Beniamine
 * Author: David Beniamine <David.Beniamine@imag.fr>
 */
/* #define MOCA_DEBUG */

#include <linux/init.h>
#include <linux/kthread.h>
#include <asm-generic/atomic-long.h>
#include "moca.h"
#include "moca_page.h"
#include "moca_probes.h"
#include "moca_tasks.h"
#include "moca_false_pf.h"

#define MOCA_DEFAULT_SCHED_PRIO 99

/* Informations about the module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Beniamine David.Beniamine@imag.fr");
MODULE_DESCRIPTION("Memory Organisation Cartography & Analysis");

/* Parameters */

// PID of the main program to track
int Moca_mainPid=0;
// Wakeup period in ms (defined in moca_tlb.c)
extern int Moca_wakeupInterval;
extern int Moca_taskDataHashBits;
extern int Moca_taskDataChunkSize;
extern int Moca_nbChunks;
extern int Moca_use_false_pf;
// Priority for FIFO scheduler
int Moca_schedulerPriority=MOCA_DEFAULT_SCHED_PRIO;
int Moca_Activated=0;

module_param(Moca_mainPid, int, 0);
module_param(Moca_wakeupInterval,int,0);
module_param(Moca_schedulerPriority,int,0);
module_param(Moca_taskDataHashBits,int,0);
module_param(Moca_taskDataChunkSize,int,0);
module_param(Moca_nbChunks,int,0);
module_param(Moca_use_false_pf,int,0);

// Thread task representation
struct task_struct *Moca_threadTask=NULL;

// Vector clock
atomic_long_t Moca_threadClock=ATOMIC_LONG_INIT(0);
void Moca_UpdateClock(void)
{
    atomic_long_inc(&Moca_threadClock);
}
unsigned long Moca_GetClock(void)
{
    unsigned long ret=atomic_long_read(&Moca_threadClock);
    return ret;
}

// Initializes threads data structures
int Moca_InitThreads(void)
{
    struct sched_param param;
    param.sched_priority=Moca_schedulerPriority;
    //Creating the thread
    Moca_threadTask=kthread_create(Moca_MonitorThread, NULL,
            "Moca main thread");
    MOCA_DEBUG_PRINT("Moca kthread task %p\n", Moca_threadTask);
    if(!Moca_threadTask)
    {
        Moca_Panic("Moca Kthread create failed");
        return -1;
    }
    get_task_struct(Moca_threadTask);
    // Set priority
    sched_setscheduler(Moca_threadTask,SCHED_FIFO,&param);
    //And finally start it
    wake_up_process(Moca_threadTask);
    return 0;
}

void Moca_CleanUp(void)
{
    if(!Moca_Activated)
        return;
    MOCA_DEBUG_PRINT("Moca Unregistering probes\n");
    Moca_UnregisterProbes();
    if(Moca_threadTask && current != Moca_threadTask)
    {
        MOCA_DEBUG_PRINT("Killing thread task %p\n",Moca_threadTask);
        kthread_stop(Moca_threadTask);
        put_task_struct(Moca_threadTask);
    }
    //Clean memory
    MOCA_DEBUG_PRINT("Moca Removing shared data\n");
    Moca_CleanProcessData();
    MOCA_DEBUG_PRINT("Moca Removed shared data\n");
    MOCA_DEBUG_PRINT("Moca Removing falsepf\n");
    Moca_ClearFalsePfData();
    MOCA_DEBUG_PRINT("Moca Removed falsepf\n");
    Moca_Activated=0;
}


// Fuction called by insmod
static int __init Moca_Init(void)
{
    printk(KERN_NOTICE "Moca started monitoring pid %d\n",
            Moca_mainPid);
    Moca_InitFalsePf();
    MOCA_DEBUG_PRINT("Moca false Pf ready \n");
    //Remove previous Moca entries
    if(Moca_InitProcessManagment(Moca_mainPid)!=0)
        return -1;
    MOCA_DEBUG_PRINT("Moca common data ready \n");
    if(Moca_InitThreads()!=0)
        return -2;
    MOCA_DEBUG_PRINT("Moca threads ready \n");
    if(Moca_RegisterProbes()!=0)
        return -3;
    MOCA_DEBUG_PRINT("Moca probes ready \n");
    printk(KERN_NOTICE "Moca correctly intialized \n");
    Moca_Activated=1;
    return 0;
}

// function called by rmmod
static void __exit Moca_Exit(void)
{
    printk(KERN_NOTICE "Moca exiting\n");
    Moca_CleanUp();
    printk(KERN_NOTICE "Moca exited\n");
}

// Panic exit function
void Moca_Panic(const char *s)
{
    printk(KERN_ALERT "Moca panic:\n%s\n", s);
    Moca_CleanUp();
}

module_init(Moca_Init);
module_exit(Moca_Exit);
