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
void MemMap_ClearAllData(void);

struct task_struct *MemMap_GetTaskFromData(task_data data);

/*
 * Add data to current chunk
 * returns 0 on success
 *         1 iff addr wad already in chunk
 *        -1 if current chunk is unusable (flush required)
 */
int MemMap_AddToChunk(task_data data, void *addr,int cpu);

/*
 * Check if add is in chunkid
 * Returns  0 if addr is in chunk
 *          1 if not
 */
/* int MemMap_IsInChunk(task_data data, void *addr); */

/*
 * Update the posth entry of the current chunk
 * Add count access and update the type
 */
int MemMap_UpdateData(task_data data,int pos, int countR, int countW, int cpu);

/*
 * Start working on the next chunks
 * If required, flush data
 * returns 1 if data were flushed, 0 else
 */
int MemMap_NextChunks(task_data data);

/*
 * This function returns the pos th add in the data's current chunk
 * returns NULL if pos is invalid ( < 0 || >= nbentry in chunk)
 */
void *MemMap_AddrInChunkPos(task_data data,int pos);

#endif //__MEMMAP_TASK_DATA__
