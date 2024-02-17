#include "my_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static void* tpool_worker(void* pool);
static void tpool_terminate_threads(tpool* pool);

void tpool_add(tpool *pool, void *(*func)(void *), void *arg) {
    if(pool==NULL || func==NULL) return;
    task_t* task = (task_t*)malloc(sizeof(task_t));

    task->func=func;
    task->arg=arg;
    task->next=NULL;

    pthread_mutex_lock(&(pool->lock));
    if(pool->tasks_head==NULL){
        pool->tasks_head=pool->tasks_tail=task;
    }else{
        pool->tasks_tail->next = task;
        pool->tasks_tail = pool->tasks_tail->next;
    }
    //pthread_cond_signal(&(pool->task_added_cond));
    pthread_cond_broadcast(&(pool->task_added_cond));
    pthread_mutex_unlock(&(pool->lock));
}

void tpool_wait(tpool *pool) {
    if(pool==NULL) return;
    pthread_mutex_lock(&(pool->lock));
    while(pool->tasks_head!=NULL || pool->running_thd_count!=0)
        pthread_cond_wait(&(pool->task_done_cond), &(pool->lock));
    pthread_mutex_unlock(&(pool->lock));
}

void tpool_destroy(tpool *pool) {
    if(pool==NULL) return;
    tpool_terminate_threads(pool);
    
    task_t *task=pool->tasks_head, *next;
    while(task!=NULL){
        next=task->next;
        free(task);
        task=next;
    }

    free(pool->tids);
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->task_added_cond));
    pthread_cond_destroy(&(pool->task_done_cond));
    free(pool);
}

tpool *tpool_init(int n_threads) {
    tpool* tp = (tpool*)malloc(sizeof(tpool));
    tp->n_threads=n_threads;
    tp->tasks_head = tp->tasks_tail = NULL;
    tp->term = 0;
    tp->running_thd_count=0;
    tp->tids = (pthread_t*)malloc(sizeof(pthread_t)*tp->n_threads);
    
    pthread_mutex_init(&(tp->lock), NULL);
    pthread_cond_init(&(tp->task_added_cond), NULL);
    pthread_cond_init(&(tp->task_done_cond), NULL);
    
    // locking here might be redundant
    pthread_mutex_lock(&(tp->lock));
    for(int i=0; i<tp->n_threads; i++){
        pthread_create(&(tp->tids[i]), NULL, tpool_worker, (void*)tp);
        //pthread_detach(tp->tids[i]);
    }
    pthread_mutex_unlock(&(tp->lock));
    return tp;
}

static void* tpool_worker(void* pool){
    tpool* tp = (tpool*)pool;
    task_t* task;
    while(48763){
        //fprintf(stderr, "worker here\n");
        //fflush(stderr);
        pthread_mutex_lock(&(tp->lock));
        while(tp->term==0 && tp->tasks_head==NULL) 
            pthread_cond_wait(&(tp->task_added_cond), &(tp->lock));

        if(tp->term) break;
        
        // tp->tasks_head should NOT be NULL
        task = tp->tasks_head;
        tp->tasks_head = tp->tasks_head->next;
        if(tp->tasks_head==NULL) tp->tasks_tail=NULL;
        
        tp->running_thd_count++;
        pthread_mutex_unlock(&(tp->lock));

        task->func(task->arg);
        
        pthread_mutex_lock(&(tp->lock));
        free(task);
        tp->running_thd_count--;
        if(tp->tasks_head==NULL && tp->running_thd_count==0)
            pthread_cond_signal(&(tp->task_done_cond));

        //fprintf(stderr, "worker done\n");
        //fflush(stderr);

        pthread_mutex_unlock(&(tp->lock));
    } 
    //fprintf(stderr, "worker end\n");
    //fflush(stderr);
    pthread_mutex_unlock(&(tp->lock));
    pthread_exit(NULL);
}

static void tpool_terminate_threads(tpool* pool){
    // make sure all threads are not running and task queue is empty

    if(pool==NULL) return;
    pthread_t tid;
    pthread_mutex_lock(&(pool->lock));
    pool->term=1;
    pthread_cond_broadcast(&(pool->task_added_cond));
    pthread_mutex_unlock(&(pool->lock));

    /* join all threads,i.e., wait for all threads to terminate */ 
    // pthread_mutex_lock(&(pool->lock));
    for(int i=0; i<pool->n_threads; i++){
        tid=pool->tids[i];
        // pthread_mutex_unlock(&(pool->lock));
        pthread_join(tid, NULL);
        // pthread_mutex_lock(&(pool->lock));
    }
    // pthread_mutex_unlock(&(pool->lock));

}
