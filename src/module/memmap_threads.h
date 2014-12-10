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
#define __NO_VERSION__
#ifndef __MEMMAP_THREADS__
#define __MEMMAP_THREADS__

//#include <linux/atomic.h>
#include <linux/smp.h> //for total_cpus

#define MEMMAP_DEFAULT_SCHED_PRIO 99

// Priority for FIFO scheduler
extern int MemMap_schedulerPriority;
extern int MemMap_pinProc;

// Initializes threads data structures
int MemMap_InitThreads(void);
// Kill all remaining kthreads, and remove their memory
void MemMap_StopThreads(void);
void MemMap_CleanThreads(void);
//Clocks managment
void MemMap_UpdateClock(int id);
void MemMap_GetClocks(unsigned long long *dst);

//TODO: addfunction to manage clock vectors
#endif //__MEMMAP_THREADS__
