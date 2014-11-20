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
#include "memmap_threads.h"
#include "memmap_taskdata.h"

#define MEMMAP_DEFAULT_MAX_PROCESS 32

// The first bits are not random enough, 14 bits should be enough for pids
#define MEMMAP_PID_HASH_BITS 14UL
#define MEMMAP_HASH_SIZE (1UL<<MEMMAP_PID_HASH_BITS)


// Monitored process
task_data *MemMap_tasksData;
int MemMap_tasksMap[MEMMAP_HASH_SIZE];
int MemMap_initPid=-1;

// Current number of monitored pids
int MemMap_numTasks=0;
// Maximum Number of monitored pids (aka current alloc size on MemMap_tasks
int MemMap_maxTasks=MEMMAP_DEFAULT_MAX_PROCESS;

spinlock_t MemMap_tasksLock;

int MemMap_AddTask(struct task_struct *t, int id);

int MemMap_InitProcessManagment(int maxprocs, int id)
{
    // Monitored pids
    int max,i;
    spin_lock_init(&MemMap_tasksLock);
    // Max allowed procs
    if(maxprocs > MEMMAP_DEFAULT_MAX_PROCESS)
    {
        max=maxprocs;
    }
    else
    {
        max=MEMMAP_DEFAULT_MAX_PROCESS;
    }
    spin_lock(&MemMap_tasksLock);
    MemMap_maxTasks=max;
    spin_unlock(&MemMap_tasksLock);


    MemMap_tasksData=kcalloc(max,sizeof(void *), GFP_KERNEL);
    if( !MemMap_tasksData)
    {
        MemMap_Panic("Tasks Alloc failed");
        return -1;
    }

    // Clear the hasmap
    for(i=0;i< MEMMAP_HASH_SIZE;++i)
        MemMap_tasksMap[i]=0;

    MemMap_initPid=id;
    return 0;

}

void MemMap_CleanProcessData(void)
{
    int i, nbTasks;
    //Tell the kernel we don't need the tasks anymore
    if(MemMap_tasksData)
    {
        printk(KERN_WARNING "MemMap Cleaning data\n");
        nbTasks=MemMap_GetNumTasks();
        for(i=0;i<nbTasks;++i)
        {
            MemMap_ClearData(MemMap_tasksData[i]);
        }
        printk(KERN_WARNING "MemMap Cleaning all data\n");
        kfree(MemMap_tasksData);
    }
}

// Add pid to the monitored process if pid is a monitored process
int MemMap_AddTaskIfNeeded(int id)
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


    ppid=task->real_parent->pid;
    //The task is a direct child of the init task
    if(ppid == MemMap_initPid)
        return MemMap_AddTask(task,id);
    //Check if the parent is known
    h=hash_32(ppid, MEMMAP_PID_HASH_BITS);
    spin_lock(&MemMap_tasksLock);
    tmpid=MemMap_tasksMap[h];
    spin_unlock(&MemMap_tasksLock);
    if (tmpid==ppid)
        return MemMap_AddTask(task,id);
    return 0;
}

// Current number of monitored pids
int MemMap_GetNumTasks(void)
{
    int nb;
    spin_lock(&MemMap_tasksLock);
    nb=MemMap_numTasks;
    spin_unlock(&MemMap_tasksLock);
    return nb;
}

int MemMap_IsMonitoredPid(int pid)
{
    u32 h= hash_32(pid,MEMMAP_PID_HASH_BITS);
    int ret;
    spin_lock(&MemMap_tasksLock);
    ret=MemMap_tasksMap[h]==pid;
    spin_unlock(&MemMap_tasksLock);
    return ret;
}

// Add t to the monitored pids
int MemMap_AddTask(struct task_struct *t, int id)
{
    task_data data;
    u32 h;
    printk(KERN_WARNING "MemMap Adding task %d %p\n",id,t );

    h= hash_32(id,MEMMAP_PID_HASH_BITS);

    //Create the task data
    data=MemMap_InitData(t);
    if(!data)
        return -1;
    //Get number and max of pids
    spin_lock(&MemMap_tasksLock);
    if(MemMap_numTasks>=MemMap_maxTasks)
    {
        spin_unlock(&MemMap_tasksLock);
        MemMap_ClearData(data);
        MemMap_Panic("Too many pids");
        return -1;
    }
    //Check if the task must be added
    if(MemMap_tasksMap[h]!=0)
    {
        spin_unlock(&MemMap_tasksLock);
        MemMap_ClearData(data);
        MemMap_Panic("MemMap Hash conflict");
        return -1;
    }
    MemMap_tasksMap[h]=id;
    MemMap_tasksData[MemMap_numTasks]=data;
    ++MemMap_numTasks;
    spin_unlock(&MemMap_tasksLock);

    printk(KERN_WARNING "MemMap Added task %d %p\n",id, t);
    return 0;
}


