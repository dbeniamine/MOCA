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
#ifndef __MEMMAP_TASK_DATA__
#define __MEMMAP_TASK_DATA__

typedef struct _task_data *task_data;

// Current number of monitored tasks
extern int MemMap_GetNumTasks(void);

task_data MemMap_InitData(struct task_struct *t);
void MemMap_ClearData(task_data data);

struct task_struct *MemMap_GetTaskFromData(task_data data);

/*
 * Add data to chunk chunkid
 * returns 0 on success
 *         1 iff addr wad already in chunk
 *        -1 if chunkid is not a valid chunk
 */
int MemMap_AddToChunk(task_data data, void *addr,int cpu, int chunkid);

/*
 * Check if add is in chunkid
 * Returns  0 if addr is in chunk
 *          1 if not
 *         -1 if chunkid is not a valid chunk
 */
int MemMap_IsInChunk(task_data data, void *addr, int chunkid);

// Return the chunkids of the current or previous chunks
int MemMap_CurrentChunk(task_data data);
int MemMap_PreviousChunk(task_data data);

/*
 * Update the posth entry of the chunk chunkid
 * Add count access and update the type
 */
int MemMap_UpdateData(task_data data,int pos, int countR, int countW, int chunkid,
        int cpu);

/*
 * Start working on the next chunks
 * If required, flush data
 * returns 1 if data were flushed, 0 else
 */
int MemMap_NextChunks(task_data data);

/*
 * This function returns the pos th add in the data's chunk number chunkid
 * returns NULL if pos is invalid ( < 0 || >= nbentry in chunk)
 */
void *MemMap_AddrInChunkPos(task_data data,int pos, int chunkid);

/*
 * None of the function above are atomic, however the following calls allows
 * you to ensure mutual exclusion when it is required
 */
void MemMap_LockData(task_data data);
void MemMap_unLockData(task_data data);
#endif //__MEMMAP_TASK_DATA__
