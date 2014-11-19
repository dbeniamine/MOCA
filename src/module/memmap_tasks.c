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
#include <linux/cpumask.h> 
#include "memmap.h"
#include "memmap_threads.h"

#define MEMMAP_DEFAULT_MAX_PROCESS 32

// The first bits are not random enough, 14 bits should be enough for pids
#define MEMMAP_PID_HASH_BITS 14UL
#define MEMMAP_HASH_SIZE (1UL<<MEMMAP_PID_HASH_BITS)


// Monitored process
struct task_struct ***MemMap_tasks;
int *MemMap_tasksPerCPU;
int MemMap_tasksMap[MEMMAP_HASH_SIZE];
int MemMap_initPid=-1;

// Current number of monitored pids
static atomic_t MemMap_numTasks=ATOMIC_INIT(0);
// Maximum Number of monitored pids (aka current alloc size on MemMap_tasks
static atomic_t MemMap_maxTasks=ATOMIC_INIT(MEMMAP_DEFAULT_MAX_PROCESS);

spinlock_t MemMap_tasksLock;
spinlock_t MemMap_tasksListLock;
spinlock_t MemMap_pinLock;
int MemMap_nextPinProc=0;
int MemMap_pinProc=1;

// Add t to the monitored pids
int MemMap_AddTask(struct task_struct *t, int id)
{
    int max, nb,cpu;
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
    // This call tells the kernel that we use this task struct
    get_task_struct(t);
    spin_unlock(&MemMap_tasksLock);


    if(MemMap_pinProc)
    {
        spin_lock(&MemMap_pinLock);
        printk(KERN_WARNING "Pinning task %d %p on proc %d\n", id, t, MemMap_nextPinProc);
        cpu=MemMap_nextPinProc;
        MemMap_nextPinProc=(MemMap_nextPinProc+1)%MemMap_NumThreads();
        spin_unlock(&MemMap_pinLock);
        set_cpus_allowed(t,*get_cpu_mask(cpu));
    }
    cpu=cpumask_first(tsk_cpus_allowed(t));
    printk(KERN_WARNING "Task %d %p is on proc %d\n", id, t, cpu);
    //Add the task to the right queue
    spin_lock(&MemMap_tasksListLock);
    MemMap_tasks[cpu][MemMap_tasksPerCPU[cpu]]=t;
    ++MemMap_tasksPerCPU[cpu];
    spin_unlock(&MemMap_tasksListLock);
    atomic_inc(&MemMap_numTasks);

    printk(KERN_WARNING "MemMap Added task %d %p\n",id, t);
    return 0;
}


int MemMap_InitProcessManagment(int maxprocs, int id)
{
    // Monitored pids
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
    spin_lock_init(&MemMap_tasksListLock);
    spin_lock_init(&MemMap_pinLock);
    MemMap_tasksPerCPU=kcalloc(MemMap_NumThreads(),sizeof(int), GFP_KERNEL);
    if( !MemMap_tasksPerCPU)
    {
        MemMap_Panic("Tasks per CPU Alloc failed");
        return -1;
    }
    MemMap_tasks=kcalloc(MemMap_NumThreads(),sizeof(void *), GFP_KERNEL);
    if( !MemMap_tasks)
    {
        MemMap_Panic("Tasks Alloc level 1 failed");
        return -1;
    }
    for (i=0;i<MemMap_NumThreads();i++)
    {
        MemMap_tasks[i]=kcalloc(max,sizeof(void *), GFP_KERNEL);
        if( !MemMap_tasks[i])
        {
            MemMap_Panic("Tasks Alloc level 2 failed");
            return -1;
        }
    }

    // empty the hasmap
    for(i=0;i< MEMMAP_HASH_SIZE;++i)
        MemMap_tasksMap[i]=0;

    MemMap_initPid=id;
    /* if( !pid) */
    /* { */
    /*     rcu_read_unlock(); */
    /*     MemMap_Panic("Main pid is NULL"); */
    /*     return -1; */
    /* } */
    /* task=pid_task(pid, PIDTYPE_PID); */
    /* rcu_read_unlock(); */
    /* if(!task) */
    /* { */
    /*     MemMap_Panic("Process without task"); */
    /*     return -1; */
    /* } */
    /* MemMap_AddTask(task, id); */
    return 0;

}

// Current number of monitored pids
int MemMap_GetNumTasks(int cpu)
{
    int nb;
    spin_lock(&MemMap_tasksListLock);
    nb=MemMap_tasksPerCPU[cpu];
    spin_unlock(&MemMap_tasksListLock);
    return nb;
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

void MemMap_CleanProcessData(void)
{
    int i,j, nbTasks;;
    //Tell the kernel we don't need the tasks anymore
    if(MemMap_tasks)
    {
        printk(KERN_WARNING "MemMap Cleaning data\n");
        for(i=0;i<MemMap_NumThreads();++i)
        {
            nbTasks=MemMap_GetNumTasks(i);
            for(j=0;j<nbTasks;++j)
                put_task_struct(MemMap_tasks[i][j]);
            printk(KERN_WARNING "MemMap Cleaning data on cpu %d\n",i);
            kfree(MemMap_tasks[i]);
        }
        printk(KERN_WARNING "MemMap Cleaning all data\n");
        kfree(MemMap_tasks);
    }
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
