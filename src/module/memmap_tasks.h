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
#ifndef __MEMMAP_TASKS__
#define __MEMMAP_TASKS__
#include "memmap_taskdata.h"
#include "memmap_hashmap.h"
// Monitored process
typedef struct _memmap_task
{
    void *key; //struct task*
    int next;
    task_data data;
}*memmap_task;

int MemMap_InitProcessManagment(int mainpid);
void MemMap_CleanProcessData(void);

/*
 * If t should be monitored, add the task to the monitored list and return
 *      the memmap_task
 * else returns NULL without doing anything
 */
memmap_task MemMap_AddTaskIfNeeded(struct task_struct *t);

// Current number of monitored tasks
int MemMap_GetNumTasks(void);

memmap_task MemMap_NextTask(int *pos);
task_data MemMap_GetData(struct task_struct *t);

void MemMap_RemoveTask(struct task_struct *t);
#endif //__MEMMAP_TASKS__
