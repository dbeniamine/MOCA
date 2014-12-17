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
extern hash_map MemMap_tasksMap;

int MemMap_InitProcessManagment(int mainpid);
void MemMap_CleanProcessData(void);

// Add the process id if its parent is already monitored
int MemMap_AddTaskIfNeeded(unsigned long int id);

// Current number of monitored tasks
int MemMap_GetNumTasks(void);
task_data MemMap_GetData(struct task_struct *t);

void MemMap_RemoveTask(struct task_struct *t);
#endif //__MEMMAP_TASKS__
