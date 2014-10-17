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

#define MEMMAP_DEFAULT_MAX_PROCESS 128

// The first bits are not random enough, 14 bits should be enough for pids
#define MEMMAP_PID_HASH_BITS 14UL
#define MEMMAP_HASH_SIZE (1UL<<MEMMAP_PID_HASH_BITS)


// Monitored process
struct task_struct **MemMap_tasks;
int MemMap_tasksMap[MEMMAP_HASH_SIZE];

// Current number of monitored pids
static atomic_t MemMap_numTasks=ATOMIC_INIT(0);
// Maximum Number of monitored pids (aka current alloc size on MemMap_tasks
static atomic_t MemMap_maxTasks=ATOMIC_INIT(MEMMAP_DEFAULT_MAX_PROCESS);

spinlock_t MemMap_tasksLock;

// Add t to the monitored pids
int MemMap_AddTask(struct task_struct *t, int id)
{
    int max, nb;
    u32 h;
    printk(KERN_WARNING "MemMap Adding task %d %p\n",id,t );

    //Get number and max of pids
    max=atomic_read(&MemMap_maxTasks);
    nb=atomic_read(&MemMap_numTasks);
    if(nb==max)
    {
        MemMap_Panic("Too many pids");
        return -1;
    }
    //Check if the task must be added
    h= hash_32(id,MEMMAP_PID_HASH_BITS);
    spin_lock(&MemMap_tasksLock);
    if(MemMap_tasksMap[h]!=0)
    {
        spin_unlock(&MemMap_tasksLock);
        MemMap_Panic("MemMap Hash conflict");
        return -1;
    }
    MemMap_tasksMap[h]=id;
    //Add the task
    MemMap_tasks[nb]=t;
    atomic_inc(&MemMap_numTasks);
    // This call tells the kernel that we use this task struct
    get_task_struct(t);
    spin_unlock(&MemMap_tasksLock);
    printk(KERN_WARNING "MemMap Added task %d %p\n",id, t);
    return 0;
}


int MemMap_InitProcessManagment(int maxprocs, int id)
{
    // Monitored pids
    struct pid *pid;
    struct task_struct *task;
    int max,i;
    // Max allowed procs
    if(maxprocs > MEMMAP_DEFAULT_MAX_PROCESS)
    {
        max=maxprocs;
        atomic_add_unless(&MemMap_maxTasks,maxprocs - MEMMAP_DEFAULT_MAX_PROCESS, 0);
    }
    else
    {
        max=MEMMAP_DEFAULT_MAX_PROCESS;
    }

    spin_lock_init(&MemMap_tasksLock);
    MemMap_tasks=kcalloc(max,sizeof(void *), GFP_KERNEL);
    if( !MemMap_tasks)
    {
        MemMap_Panic("Tasks Alloc failed");
        return -1;
    }

    // empty the hasmap
    for(i=0;i< MEMMAP_HASH_SIZE;++i)
        MemMap_tasksMap[i]=0;

    rcu_read_lock();
    pid=find_vpid(id);
    if( !pid)
    {
        rcu_read_unlock();
        MemMap_Panic("Main pid is NULL");
        return -1;
    }
    task=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();
    if(!task)
    {
        MemMap_Panic("Process without task");
        return -1;
    }
    MemMap_AddTask(task, id);
    return 0;

}

// Current number of monitored pids
int MemMap_GetNumTasks(void)
{
    return atomic_read(&MemMap_numTasks);
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
    h=hash_32(ppid, MEMMAP_PID_HASH_BITS);
    spin_lock(&MemMap_tasksLock);
    tmpid=MemMap_tasksMap[h];
    spin_unlock(&MemMap_tasksLock);
    if (tmpid==ppid)
        MemMap_AddTask(task,id);
    return 0;
}

void MemMap_CleanProcessData(void)
{
    int i, nbTasks=MemMap_GetNumTasks();
    //Tell the kernel we don't need the tasks anymore
    for(i=0;i<nbTasks;++i)
        put_task_struct(MemMap_tasks[i]);
    if(MemMap_tasks)
        kfree(MemMap_tasks);
}
