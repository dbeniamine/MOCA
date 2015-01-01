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
#ifndef __MEMMAP_LOCK__
#define __MEMMAP_LOCK__

/*
 * This file provides a lock with two priority level base on linux/spinlock.h
 */
#define MEMMAP_LOCK_PRIO_MIN 0
#define MEMMAP_LOCK_PRIO_MAX 1

typedef struct _memmap_lock *MemMap_Lock_t;
MemMap_Lock_t MemMap_Lock_Init(void);
void MemMap_Lock(MemMap_Lock_t lock, int prio);
void MemMap_Unlock(MemMap_Lock_t);
#endif //__MEMMAP_LOCK__
