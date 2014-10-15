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

#include <linux/kprobes.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include "memmap.h"

int MemMap_ForkHandler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    int pid = regs_return_value(regs);
    struct pid *pids;
    struct task_struct *task = NULL;
    printk(KERN_WARNING "MemMap ForkHandler pid %d\n", pid);
    rcu_read_lock();
    pids = find_vpid(pid);
    rcu_read_unlock();
    printk(KERN_WARNING "MemMap ForkHandler pids %p\n", pids);
    if (pids)
    {
        task = pid_task(pids, PIDTYPE_PID);
        printk(KERN_WARNING "MemMap ForkHandler task %p\n", task);
    }

    if (task && MemMap_parentPid == task->parent->pid)
        MemMap_AddPid(pids);
    printk(KERN_WARNING "MemMap ForkHandler ended\n");

    return 0;
}

static struct kretprobe MemMap_ForkProbe = {
    .handler = MemMap_ForkHandler,
    .kp.symbol_name = "do_fork",
};


void MemMap_RegisterProbes(void)
{
    int ret;
    if ((ret=register_kretprobe(&MemMap_ForkProbe))){
        MemMap_Panic("Unable to register fork prove");
    }
}


void MemMap_UnregisterProbes(void)
{
    unregister_kretprobe(&MemMap_ForkProbe);
}
