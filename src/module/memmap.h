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
#ifndef __MEMMAP__
#define __MEMMAP__

#include <linux/module.h>
#include <linux/kernel.h>

// Monitored tasked
extern struct pid **MemMap_pids;
extern pid_t MemMap_parentPid;

// Panic exit function
void MemMap_Panic(const char *s);

// Returns the current number of monitored pids
int MemMap_GetNumPids(void);

// Add t to the monitored pids
int MemMap_AddPid(struct pid *p);

#endif //__MEMMAP__

