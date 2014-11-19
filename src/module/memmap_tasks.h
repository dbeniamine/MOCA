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

// Monitored process
extern struct task_struct ***MemMap_tasks;

int MemMap_InitProcessManagment(int maxprocs, int mainpid);
void MemMap_CleanProcessData(void);

// Add the process id if its parent is already monitored
int MemMap_AddTaskIfNeeded(int id);

// Current number of monitored on a cpu
int MemMap_GetNumTasks(int cpu);
//Is the process pid monitored by memmap ?
int MemMap_IsMonitoredPid(int pid);
#endif //__MEMMAP_TASKS__
