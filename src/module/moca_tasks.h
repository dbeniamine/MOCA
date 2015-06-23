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
#ifndef __MOCA_TASKS__
#define __MOCA_TASKS__
#include "moca_taskdata.h"
#include "moca_hashmap.h"
// Monitored process
typedef struct _moca_task
{
    void *key; //struct task*
    int next;
    task_data data;
}*moca_task;

int Moca_InitProcessManagment(int mainpid);
void Moca_CleanProcessData(void);

/*
 * If t should be monitored, add the task to the monitored list and return
 *      the moca_task
 * else returns NULL without doing anything
 */
moca_task Moca_AddTaskIfNeeded(struct task_struct *t);

// Current number of monitored tasks
int Moca_GetNumTasks(void);

moca_task Moca_NextTask(int *pos);
task_data Moca_GetData(struct task_struct *t);

void Moca_RemoveTask(struct task_struct *t);

int Moca_IsTrackedMm(struct mm_struct *mm);
#endif //__MOCA_TASKS__
