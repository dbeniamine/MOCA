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

#ifndef ATOMIC_H
#define ATOMIC_H

#include <pthread.h>

typedef struct {
	int counter;
} atomic_t;

typedef struct {
	long counter;
} atomic_long_t;

// This is a very ugly hack
pthread_mutex_t atomic_long_mutex;
pthread_mutex_t atomic_mutex;

static inline void init_atomic(void)
{
    pthread_mutex_init(&atomic_mutex,NULL);
    pthread_mutex_init(&atomic_long_mutex,NULL);
}

#define ATOMIC_INIT(i)	{ (i) }
#define ATOMIC_LONG_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
static inline int atomic_read(const atomic_t *v)
{
    int ret=0;
    pthread_mutex_lock(&atomic_mutex);
    ret=v->counter;
    pthread_mutex_unlock(&atomic_mutex);
	return ret;
}

static inline int atomic_inc_return(atomic_t *v)
{
    int ret;
    pthread_mutex_lock(&atomic_mutex);
    ret=++v->counter;
    pthread_mutex_unlock(&atomic_mutex);
    return ret;
}

static inline void atomic_inc(atomic_t *v)
{
    atomic_inc_return(v);
}

static inline void atomic_set(atomic_t *v,int c)
{
    pthread_mutex_lock(&atomic_mutex);
    v->counter=c;
    pthread_mutex_unlock(&atomic_mutex);
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.
 */
static inline void atomic_dec(atomic_t *v)
{
    pthread_mutex_lock(&atomic_mutex);
    --v->counter;
    pthread_mutex_unlock(&atomic_mutex);
}

static inline long atomic_long_read(const atomic_long_t *v)
{
    long ret=0;
    pthread_mutex_lock(&atomic_long_mutex);
    ret=v->counter;
    pthread_mutex_unlock(&atomic_long_mutex);
	return ret;
}

static inline void atomic_long_inc(atomic_long_t *v)
{
    pthread_mutex_lock(&atomic_long_mutex);
    ++v->counter;
    pthread_mutex_unlock(&atomic_long_mutex);
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.
 */
static inline void atomic_long_dec(atomic_long_t *v)
{
    pthread_mutex_lock(&atomic_long_mutex);
    --v->counter;
    pthread_mutex_unlock(&atomic_long_mutex);
}

#endif /* _ASM_X86_ATOMIC_H */
