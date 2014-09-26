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

#include <linux/atomic.h>

/* Maximum number of threads used by the monitored application
 * The application must not create more thread than this number
 * If the monitored application uses more than Memmap_numthreads threads, the
* module will abort
*/
static int MemMap_numThreads=0;
// Current number of monitored threads
static atomic_t MemMap_activeThreads=0
static int * MemMap_threadClocks=NULL;

// Initializes threads data structures
void MemMap_InitThreads(void);
// Start a new thread if needed
void MemMap_NewThread(void);
// Kill all remaining kthreads, and remove their memory
void MemMap_CleanThreads(void);

#endif //__MEMMAP_THREADS__
