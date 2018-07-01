#pragma once

#define THREADS_LIMITS 55 
#define QUEUE_LIMITS 55555 

typedef struct threadpool_t threadpool_t;

typedef enum {
    threadpool_invalid        = -1,
    threadpool_lock_failure   = -2,
    threadpool_queue_full     = -3,
    threadpool_shutdown       = -4,
    threadpool_thread_failure = -5
} threadpool_failure_t;

typedef enum {
    threadpool_specific_way   = 1
} threadpool_destroy_t;

threadpool_t *threadpool_create(int thread_count, int queue_size, int arg);

int threadpool_add(threadpool_t *pool, void (*function)(void *), void *arg, int args);

int threadpool_destroy(threadpool_t *pool, int arg);
