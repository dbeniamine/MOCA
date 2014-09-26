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
#ifndef __MEMMAP_STATE__
#define __MEMMAP_STATE__

#define MEMMAP_DEFAULT_WAKEUP_INTERVAL 200
#define MEMMAP_DEFAULT_SCHED_PRIO 99

// PID of the main program to track
static int MemMap_mainPid=0;
// Wakeup period in ms
static int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;
// Priority for FIFO scheduler
static int MemMap_schedulerPriority=MEMMAP_DEFAULT_SCHED_PRIO;

// Panic exit function
void MemMap_Panic(const char *s);

#endif //__MEMMAP_STATE__

