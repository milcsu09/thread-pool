#include "thread-pool.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>


struct thread_pool_work
{
  void (*function) (void *);
  void  *argument;
};


struct thread_pool
{
  pthread_mutex_t          mutex;
  pthread_cond_t           cond_work;
  pthread_cond_t           cond_done;

  pthread_t               *threads;
  size_t                   threads_amount;
  size_t                   threads_active;

  struct thread_pool_work *queue;
  size_t                   queue_capacity;
  size_t                   queue_size;
  size_t                   queue_head;
  size_t                   queue_tail;

  int                      stop;
};


static void *thread_pool_thread_work (void *);


static void
thread_pool_work_enqueue (struct thread_pool *pool,
                          struct thread_pool_work work)
{
  pool->queue_size++;

  pool->queue[pool->queue_tail] = work;
  pool->queue_tail = (pool->queue_tail + 1) % pool->queue_capacity;
}


static void
thread_pool_work_dequeue (struct thread_pool *pool,
                          struct thread_pool_work *work)
{
  pool->queue_size--;

  *work = pool->queue[pool->queue_head];
  pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
}


struct thread_pool *
thread_pool_create (size_t threads_amount, size_t queue_capacity)
{
  struct thread_pool *pool;

  pool = malloc (sizeof (struct thread_pool));

  pthread_mutex_init (&pool->mutex, NULL);
  pthread_cond_init (&pool->cond_work, NULL);
  pthread_cond_init (&pool->cond_done, NULL);

  pool->threads = calloc (threads_amount, sizeof (pthread_t));
  pool->threads_amount = threads_amount;
  pool->threads_active = 0;

  pool->queue = calloc (queue_capacity, sizeof (struct thread_pool_work));
  pool->queue_capacity = queue_capacity;
  pool->queue_size = 0;
  pool->queue_head = 0;
  pool->queue_tail = 0;

  pool->stop = 0;

  for (size_t i = 0; i < threads_amount; ++i)
    pthread_create (&pool->threads[i], NULL, thread_pool_thread_work, pool);

  return pool;
}


void
thread_pool_destroy (struct thread_pool *pool)
{
  pthread_mutex_lock (&pool->mutex);

  pool->stop = 1;
  pthread_cond_broadcast (&pool->cond_work);

  pthread_mutex_unlock (&pool->mutex);

  for (size_t i = 0; i < pool->threads_amount; ++i)
    pthread_join (pool->threads[i], NULL);

  pthread_mutex_destroy (&pool->mutex);
  pthread_cond_destroy (&pool->cond_work);
  pthread_cond_destroy (&pool->cond_done);

  free (pool->threads);
  free (pool->queue);

  free (pool);
}


void
thread_pool_clear (struct thread_pool *pool)
{
  pthread_mutex_lock (&pool->mutex);

  pool->queue_size = 0;
  pool->queue_head = 0;
  pool->queue_tail = 0;

  pthread_cond_signal(&pool->cond_done);

  pthread_mutex_unlock (&pool->mutex);
}


int
thread_pool_enqueue (struct thread_pool *pool, void (*function) (void *),
                     void *argument)
{
  pthread_mutex_lock (&pool->mutex);

  if (pool->stop || pool->queue_size == pool->queue_capacity)
    {
      pthread_mutex_unlock (&pool->mutex);
      return 0;
    }

  struct thread_pool_work work;

  work.function = function;
  work.argument = argument;

  thread_pool_work_enqueue (pool, work);

  pthread_cond_signal (&pool->cond_work);

  pthread_mutex_unlock (&pool->mutex);

  return 1;
}


void
thread_pool_wait (struct thread_pool *pool)
{
  pthread_mutex_lock (&pool->mutex);

  while (pool->queue_size > 0 || pool->threads_active > 0)
    pthread_cond_wait (&pool->cond_done, &pool->mutex);

  pthread_mutex_unlock (&pool->mutex);
}


int
thread_pool_is_done (struct thread_pool *pool)
{
  int done;

  pthread_mutex_lock (&pool->mutex);
  done = pool->queue_size == 0 && pool->threads_active == 0;
  pthread_mutex_unlock (&pool->mutex);

  return done;
}


static void *
thread_pool_thread_work (void *argument)
{
  struct thread_pool *pool = argument;

  while (1)
    {
      pthread_mutex_lock (&pool->mutex);

      while (!pool->stop && pool->queue_size == 0)
        pthread_cond_wait (&pool->cond_work, &pool->mutex);

      if (pool->stop && pool->queue_size == 0)
        {
          pthread_mutex_unlock (&pool->mutex);
          break;
        }

      struct thread_pool_work work;

      thread_pool_work_dequeue (pool, &work);

      pool->threads_active++;

      pthread_mutex_unlock (&pool->mutex);

      if (work.function)
        work.function (work.argument);

      pthread_mutex_lock (&pool->mutex);

      pool->threads_active--;

      if (pool->queue_size == 0 && pool->threads_active == 0)
        pthread_cond_signal (&pool->cond_done);

      pthread_mutex_unlock (&pool->mutex);
    }

  return NULL;
}

