/*
 * Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
 * Author: Beniamine, David <David@Beniamine.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MOCA_TASKS__
#define __MOCA_TASKS__
#include "moca_taskdata.h"
#include "moca_hashmap.h"
// Monitored process
typedef struct _moca_task
{
    void *key; //struct task*
    int next;
    task_data data;
    int touched;
}*moca_task;

int Moca_InitProcessManagment(int mainpid);
void Moca_CleanProcessData(void);

/*
 * If t should be monitored, add the task to the monitored list and return
 *      the moca_task
 * else returns NULL without doing anything
 */
moca_task Moca_AddTaskIfNeeded(struct task_struct *t);

// Current number of monitored tasks
int Moca_GetNumTasks(void);

moca_task Moca_NextTask(int *pos);
task_data Moca_GetData(struct task_struct *t);

void Moca_RemoveTask(struct task_struct *t);

// Mark tsk as touched, return 1 ifit is the first touch, 0 else
int Moca_FirstTouchTask(moca_task tsk);
#endif //__MOCA_TASKS__
