#include "threadtools.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call siglongjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    sigprocmask(SIG_SETMASK, &base_mask, NULL); // make sure SIGALRM and SIGTSTP are blocked

    if(signo==SIGTSTP){
        printf("caught SIGTSTP\n");
    }else{
        printf("caught SIGALRM\n");
        alarm(timeslice);
    }
    longjmp(sched_buf, 1);
}


/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    int ret=setjmp(sched_buf);
    if(ret==0){
        rq_size=wq_size=0;
        while(ready_queue[rq_size]!=NULL) rq_size++;
        if(rq_size>0){
            rq_current=0;
            longjmp(RUNNING->environment, 1);
        }else return;
    }

    // else
    if(bank.lock_owner==-1 && wq_size>0){
        bank.lock_owner=waiting_queue[0]->id;
        ready_queue[rq_size++]=waiting_queue[0];
        for(int i=0;i<=wq_size-2;i++) waiting_queue[i]=waiting_queue[i+1];
        wq_size--;
    }
    if(ret==2 || ret==3){
        if(ret==2) waiting_queue[wq_size++]=ready_queue[rq_current]; // lock
        else free(ready_queue[rq_current]); // thread_exit
        if(rq_current<rq_size-1) ready_queue[rq_current]=ready_queue[rq_size-1];
        else rq_current=0;
        ready_queue[rq_size-1]=NULL;
        rq_size--;
    }else{
        // thread_yield
        rq_current=(rq_size>0)?((rq_current+1)%rq_size):0;
    }
    if(rq_size==0) return;
    longjmp(RUNNING->environment, 1);
}

    
