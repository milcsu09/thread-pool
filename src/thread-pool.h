#ifndef THREAD_POOL_H
#define THREAD_POOL_H


#include <stddef.h>


struct thread_pool;


struct thread_pool *thread_pool_create (size_t, size_t);

void thread_pool_destroy (struct thread_pool *);

void thread_pool_clear (struct thread_pool *);

int thread_pool_enqueue (struct thread_pool *, void (*) (void *), void *);

void thread_pool_wait (struct thread_pool *);

int thread_pool_is_done (struct thread_pool *);


#endif // THREAD_POOL_H

