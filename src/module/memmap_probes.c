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
	struct task_struct *task = NULL, *mainTask;

	rcu_read_lock();
	pids = find_vpid(pid);
	if (pids)
		task = pid_task(pids, PIDTYPE_PID);
    //Get the main task
    mainTask=pid_task(MemMap_pids[0], PIDTYPE_PID);
	rcu_read_unlock();

	if (!task)
		return 0;
    //Todo: adapt here

	if (mainTask->parent->pid == task->parent->pid)
        MemMap_AddPid(pids);

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
