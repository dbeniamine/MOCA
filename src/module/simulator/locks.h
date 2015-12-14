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

#ifndef _MOCA_LOCKS_
#define _MOCA_LOCKS_

#include <pthread.h>


typedef pthread_mutex_t spinlock_t;

typedef struct _rwlock_t
{
    pthread_mutex_t m;
    int R;  // Reader in or waiting
    int W;  // Writer in
}rwlock_t;

void spin_lock_init(spinlock_t *m);
void spin_lock(spinlock_t *m);
void spin_unlock(spinlock_t *m);

void rwlock_init(rwlock_t *rwl);
void read_lock(rwlock_t *rwl);
void read_unlock(rwlock_t *rwl);
void write_lock(rwlock_t *rwl);
void write_unlock(rwlock_t *rwl);

#endif
