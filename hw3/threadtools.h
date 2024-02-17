#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/signal.h>
#include "bank.h"


#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
typedef struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int i, x, y;  // declare the variables you wish to keep between switches
} TCB;



extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
extern struct Bank bank;
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

// TODO
#define thread_create(func, id, arg)\
    do{\
        func(id, arg);\
    } while(0)

#define thread_setup(_id, _arg)\
    do{\
        ready_queue[rq_size]=(TCB*)malloc(sizeof(TCB));\
        ready_queue[rq_size]->id=_id;\
        ready_queue[rq_size]->arg=_arg;\
        printf("%d %s\n", _id, __func__);\
        if(setjmp(ready_queue[rq_size]->environment)==0){\
            rq_size++;\
            return;\
        }\
    } while(0)

#define thread_exit()\
    do{\
        longjmp(sched_buf, 3);\
    } while(0)

#define thread_yield()\
    do{\
        if(setjmp(RUNNING->environment)==0){\
            sigprocmask(SIG_SETMASK, &alrm_mask, NULL);\
            sigprocmask(SIG_SETMASK, &tstp_mask, NULL);\
            sigprocmask(SIG_SETMASK, &base_mask, NULL);\
        }\
    } while(0)

#define lock()\
    do{\
        if(bank.lock_owner==-1) bank.lock_owner=RUNNING->id;\
        else if(setjmp(RUNNING->environment)==0) longjmp(sched_buf, 2);\
    } while(0)

#define unlock()\
    do{\
        if(bank.lock_owner==RUNNING->id) bank.lock_owner=-1;\
    } while(0)

#endif // THREADTOOL
