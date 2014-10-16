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
#ifndef __MEMMAP_PID__
#define __MEMMAP_PID__

// Monitored process
extern struct pid **MemMap_pids;

int MemMap_InitProcessManagment(int maxprocs, int mainpid);
void MemMap_CleanProcessData(void);

// Add the process id if its parent is already monitored
int MemMap_AddPidIfNeeded(int id);

// Current number of monitored pids
int MemMap_GetNumPids(void);
#endif //__MEMMAP_PID__
