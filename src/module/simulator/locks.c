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


