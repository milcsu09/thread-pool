#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <stdatomic.h>

#include "thread-pool.h"

static atomic_int g_generation;

struct data
{
  int n;
  int generation;
};

void
worker_function (void *argument)
{
  struct data *data = argument;

  while (1)
    {
      if (data->generation != atomic_load (&g_generation))
        break;

      printf ("[THREAD %d] Working\n", data->n);
      usleep (500000);
    }

  printf ("[THREAD %d] Finished\n", data->n);
  free (data);
}

int
main (void)
{
  struct thread_pool *pool;

  pool = thread_pool_create (16, 4096);

  for (int i = 0; i < 4; ++i)
    {
      struct data *data;
      data = calloc (1, sizeof (struct data));

      data->n = i;
      data->generation = atomic_load (&g_generation);

      thread_pool_enqueue (pool, worker_function, data);
    }

  int i = 0;

  while (1)
    {
      ++i;

      printf ("\t%d %d\n", i, thread_pool_get_threads_active (pool));

      if (i == 3)
        {
          atomic_fetch_add (&g_generation, 1);
          break;
        }

      usleep (1000000);
    }

  printf ("FINISHED!\n");

  thread_pool_destroy (pool);

  return 0;
}

