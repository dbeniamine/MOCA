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
#include "memmap_tlb.h"
#include "memmap_threads.h"
#include "memmap_probes.h"
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/sched.h>


/* Informations about the module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Beniamine David.Beniamine@imag.fr");
MODULE_DESCRIPTION("MemMap's kernel module tracks memory access");

/* Parameters */

// PID of the main program to track
int MemMap_mainPid=0;
// Wakeup period in ms (defined in memmap_tlb.c)
extern int MemMap_wakeupInterval;
// Monitored tasked
struct pid **MemMap_pids;
pid_t MemMap_parentPid;
spinlock_t MemMap_pidsLock;
// Current number of monitored pids
static atomic_t MemMap_numPids=ATOMIC_INIT(0);
// Maximum Number of monitored pids (aka current alloc size on MemMap_pids
static atomic_t MemMap_maxPids=ATOMIC_INIT(8);


module_param(MemMap_mainPid, int, 0);
module_param(MemMap_wakeupInterval,int,0);
module_param(MemMap_schedulerPriority,int,0);

int MemMap_InitCommonData(void)
{
    // Monitored pids
    int max=atomic_read(&MemMap_maxPids);
    struct task_struct *mainTask;

    spin_lock_init(&MemMap_pidsLock);
    printk(KERN_WARNING "MemMap before calloc max: %d\n", max);
    MemMap_pids=kcalloc(max,sizeof(void *), GFP_KERNEL);
    printk(KERN_WARNING "MemMap after calloc pids: %p\n", MemMap_pids);
    if( !MemMap_pids)
    {
        MemMap_Panic("Tasks Alloc failed");
        return -1;
    }
    printk(KERN_WARNING "MemMap before before lock \n");
    rcu_read_lock();
    printk(KERN_WARNING "MemMap in lock pid: %d\n", MemMap_mainPid);
    MemMap_pids[0]=find_vpid(MemMap_mainPid);
    if( !MemMap_pids[0])
    {
        MemMap_Panic("Main pid is NULL");
        return -1;
    }
    mainTask=pid_task(MemMap_pids[0],PIDTYPE_PID);
    if(! mainTask)
    {
        MemMap_Panic("Main task is NULL");
        return -1;
    }
    MemMap_parentPid=mainTask->parent->pid;
    rcu_read_unlock();
    printk(KERN_WARNING "MemMap out lock pids[0]: %p \n", MemMap_pids[0]);
    atomic_inc(&MemMap_numPids);
    printk(KERN_WARNING "MemMap end init common data \n");
    return 0;
}

// Current number of monitored pids
int MemMap_GetNumPids(void)
{
    return atomic_read(&MemMap_numPids);
}

// Add t to the monitored pids
int MemMap_AddPid(struct pid *p)
{
    int max, nb;
    spin_lock(&MemMap_pidsLock);
    printk(KERN_WARNING "MemMap Adding pid %p\n", p);
    //Get number and max of pids
    max=atomic_read(&MemMap_maxPids);
    nb=atomic_read(&MemMap_numPids);
    if(nb==max)
    {
        MemMap_Panic("Too many pids");
        return -1;
    }
    //Add the task
    MemMap_pids[nb]=p;
    atomic_inc(&MemMap_numPids);
    spin_unlock(&MemMap_pidsLock);
    printk(KERN_WARNING "MemMap Added pid %p\n",p);
    return 0;
}

void MemMap_CleanUp(void)
{
    MemMap_UnregisterProbes();
    //Clean common stuff
    if(MemMap_pids)
        kfree(MemMap_pids);
    // Clean all memory used by threads structure
    MemMap_CleanThreads();
}


// Fuction called by insmod
static int __init MemMap_Init(void)
{
    printk(KERN_WARNING "MemMap started monitoring pid %d\n",
            MemMap_mainPid);
    if(MemMap_InitCommonData()!=0)
        return -1;
    printk(KERN_WARNING "MemMap common data ready \n");
    if(MemMap_RegisterProbes()!=0)
        return -2;
    printk(KERN_WARNING "MemMap probes ready \n");
    if(MemMap_InitThreads()!=0)
        return -3;
    printk(KERN_WARNING "MemMap threads ready \n");
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

module_init(MemMap_Init);
module_exit(MemMap_Exit);
