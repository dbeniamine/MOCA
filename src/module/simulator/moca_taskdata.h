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
#ifndef __MOCA_TASK_DATA__
#define __MOCA_TASK_DATA__

typedef struct _task_data *task_data;

// Current number of monitored tasks
extern int Moca_GetNumTasks(void);

void Moca_InitTaskData(void);
task_data Moca_InitData(struct task_struct *t);
void Moca_ClearAllData(void);

struct task_struct *Moca_GetTaskFromData(task_data data);

/*
 * Add data to current chunk
 * returns 0 on success
 *         1 iff addr wad already in chunk
 *        -1 if current chunk is unusable (flush required)
 */
int Moca_AddToChunk(task_data data, void *addr,int cpu, int write);

/*
 * Start working on the next chunks
 * If required, flush data
 * returns the id of the old chunk
 */
int Moca_NextChunks(task_data data);

/*
 * Mark the chunk ch as ended (flushable)
 */
void Moca_EndChunk(task_data data, int ch);

/*
 * This function returns the pos th add in the data's chunk ch
 * returns NULL if pos is invalid ( < 0 || >= nbentry in chunk)
 */
void *Moca_AddrInChunkPos(task_data data,int *pos, int ch);

void Moca_LockChunk(task_data data);
void Moca_UnlockChunk(task_data data);

#endif //__MOCA_TASK_DATA__