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
