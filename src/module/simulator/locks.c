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

#include "false_moca.h"
#include "locks.h"
#include <stdio.h>

void rwlock_init(rwlock_t *rwl)
{
    pthread_mutex_init(&rwl->m,NULL);
    rwl->R=0;
    rwl->W=0;
}

void read_lock(rwlock_t *rwl)
{
    pthread_mutex_lock(&rwl->m);
    ++rwl->R;
    while(rwl->W > 0)
    {
        pthread_mutex_unlock(&rwl->m);
        msleep(1);
        pthread_mutex_lock(&rwl->m);
    }
    pthread_mutex_unlock(&rwl->m);
}

void read_unlock(rwlock_t *rwl)
{
    pthread_mutex_lock(&rwl->m);
    --rwl->R;
    pthread_mutex_unlock(&rwl->m);
}

void write_lock(rwlock_t *rwl)
{
    pthread_mutex_lock(&rwl->m);
    while(rwl->W > 0 || rwl->R > 0 )
    {
        pthread_mutex_unlock(&rwl->m);
        msleep(1);
        pthread_mutex_lock(&rwl->m);
    }
    ++rwl->W;
    pthread_mutex_unlock(&rwl->m);
}

void write_unlock(rwlock_t *rwl)
{
    pthread_mutex_lock(&rwl->m);
    --rwl->W;
    pthread_mutex_unlock(&rwl->m);
}

void spin_lock_init(spinlock_t *m)
{
    pthread_mutex_init((pthread_mutex_t *)m,NULL);
}

void spin_lock(spinlock_t *m)
{
    pthread_mutex_lock((pthread_mutex_t *)m);
}

void spin_unlock(spinlock_t *m)
{
    pthread_mutex_unlock((pthread_mutex_t *)m);
}


