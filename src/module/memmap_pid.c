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
#include <linux/spinlock.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include "memmap.h"

#define MEMMAP_DEFAULT_MAX_PROCESS 32

// The first bits are not random enough, 14 bits should be enough for pids
#define MEMMAP_PID_HASH_BITS 14UL
#define MEMMAP_HASH_SIZE (1UL<<MEMMAP_PID_HASH_BITS)


// Monitored process
struct pid **MemMap_pids;
int MemMap_pidsMap[MEMMAP_HASH_SIZE];

// Current number of monitored pids
static atomic_t MemMap_numPids=ATOMIC_INIT(0);
// Maximum Number of monitored pids (aka current alloc size on MemMap_pids
static atomic_t MemMap_maxPids=ATOMIC_INIT(MEMMAP_DEFAULT_MAX_PROCESS);

spinlock_t MemMap_pidsLock;

// Add t to the monitored pids
int MemMap_AddPid(struct pid *p, int id)
{
    int max, nb;
    u32 h;
    printk(KERN_WARNING "MemMap Adding pid %d %p\n",id,p );

    //Get number and max of pids
    max=atomic_read(&MemMap_maxPids);
    nb=atomic_read(&MemMap_numPids);
    if(nb==max)
    {
        MemMap_Panic("Too many pids");
        return -1;
    }
    h= hash_32(id,MEMMAP_PID_HASH_BITS);
    spin_lock(&MemMap_pidsLock);
    if(MemMap_pidsMap[h]!=0)
    {
        spin_unlock(&MemMap_pidsLock);
        MemMap_Panic("MemMap Hash conflict");
        return -1;
    }
    MemMap_pidsMap[h]=id;
    //Add the task
    MemMap_pids[nb]=p;
    atomic_inc(&MemMap_numPids);
    spin_unlock(&MemMap_pidsLock);
    printk(KERN_WARNING "MemMap Added pid %d %p\n",id, p);
    return 0;
}


int MemMap_InitProcessManagment(int maxprocs, int id)
{
    // Monitored pids
    struct pid *pid;
    int max;
    // Max allowed procs
    if(maxprocs > MEMMAP_DEFAULT_MAX_PROCESS)
    {
        max=maxprocs;
        atomic_add_unless(&MemMap_maxPids,maxprocs - MEMMAP_DEFAULT_MAX_PROCESS, 0);
    }
    else
    {
        max=MEMMAP_DEFAULT_MAX_PROCESS;
    }

    spin_lock_init(&MemMap_pidsLock);
    MemMap_pids=kcalloc(max,sizeof(void *), GFP_KERNEL);
    if( !MemMap_pids)
    {
        MemMap_Panic("Tasks Alloc failed");
        return -1;
    }
    rcu_read_lock();
    pid=find_vpid(id);
    rcu_read_unlock();
    if( !pid)
    {
        MemMap_Panic("Main pid is NULL");
        return -1;
    }
    MemMap_AddPid(pid, id);
    return 0;

}

// Current number of monitored pids
int MemMap_GetNumPids(void)
{
    return atomic_read(&MemMap_numPids);
}


// Add pid to the monitored process if pid is a monitored process
int MemMap_AddPidIfNeeded(int id)
{
    struct pid *pid;
    struct task_struct *task;
    int ppid, tmpid;
    u32 h;
    // Get iternal pid representation
    rcu_read_lock();
    pid=find_vpid(id);

    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        printk(KERN_WARNING "MemMap process skiped %d NULL\n", id);
        return 0;
    }
    task=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();

    if(!task)
    {
        MemMap_Panic("Process without task");
        return -1;
    }

    ppid=task->real_parent->pid;
    h=hash_32(ppid, MEMMAP_PID_HASH_BITS);
    spin_lock(&MemMap_pidsLock);
    tmpid=MemMap_pidsMap[h];
    spin_unlock(&MemMap_pidsLock);
    if (tmpid==ppid)
        MemMap_AddPid(pid,id);
    return 0;
}

void MemMap_CleanProcessData(void)
{
    if(MemMap_pids)
        kfree(MemMap_pids);
}
