#include <pthread.h>
#ifndef __MY_THREAD_POOL_H
#define __MY_THREAD_POOL_H

// #include <stdbool.h>

// typedef void* (*task_func_t)(void*);

typedef struct task {
    void* (*func)(void*);
    void* arg;
    struct task* next; 
} task_t;

typedef struct tpool {
    task_t *tasks_head, *tasks_tail;
    int n_threads;
    pthread_t* tids;
    pthread_mutex_t lock;
    pthread_cond_t task_added_cond;
    pthread_cond_t task_done_cond;
    // pthread_cond_t thread_term_cond;
    int running_thd_count;
    // int thd_count;
    int term;
} tpool;

tpool *tpool_init(int n_threads);
void tpool_add(tpool *, void *(*func)(void *), void *);
void tpool_wait(tpool *);
void tpool_destroy(tpool *);

#endif
