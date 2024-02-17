#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "threadtools.h"

int timeslice;
sigset_t base_mask, tstp_mask, alrm_mask;
struct tcb *ready_queue[], *waiting_queue[];
int rq_size, rq_current, wq_size;
jmp_buf sched_buf;
struct Bank bank;


/* prototype of the thread functions */
void fibonacci(int, int);
void factorial(int, int);
void bank_operation(int, int);

/*
 * This function turns stdin, stdout, and stderr into unbuffered I/O, so:
 *   - you see everything your program prints in case it crashes
 *   - the program behaves the same if its stdout doesn't connect to a terminal
 */
void unbuffered_io() {
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
}

/*
 * Initializes the signal masks and the signal handler.
 */
void init_signal() {
    /* initialize the signal masks */
    sigemptyset(&base_mask);
    sigaddset(&base_mask, SIGTSTP);
    sigaddset(&base_mask, SIGALRM);
    sigemptyset(&tstp_mask);
    sigaddset(&tstp_mask, SIGTSTP);
    sigemptyset(&alrm_mask);
    sigaddset(&alrm_mask, SIGALRM);

    /* initialize the signal handlers */
    signal(SIGTSTP, sighandler);
    signal(SIGALRM, sighandler);

    /* block both SIGTSTP and SIGALRM */
    sigprocmask(SIG_SETMASK, &base_mask, NULL);
}

/*
 * Threads are created nowhere else but here.
 */
void init_threads(int fib_arg, int fact_arg, int bank1_arg, int bank2_arg) {
    char padding[64];
    if (fib_arg > 0)
        thread_create(fibonacci, 0, fib_arg);
    if (fact_arg > 0)
        thread_create(factorial, 1, fact_arg);
    if (bank1_arg) 
        thread_create(bank_operation, 2, bank1_arg); 
    if (bank2_arg) 
        thread_create(bank_operation, 3, bank2_arg); 
}

/* 
 * Calls the scheduler to begin threading.
 */
void start_threading() {
    alarm(timeslice);
    scheduler();
}

void init_bank() {
    bank.balance = 500;
    bank.lock_owner = -1;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("usage: %s [timeslice] [fib_arg] [fact_arg] [bank1_arg] [bank2_arg]\n", argv[0]);
        exit(1);
    }
    timeslice = atoi(argv[1]);
    int fib_arg = atoi(argv[2]);
    int fact_arg = atoi(argv[3]);
    int bank1_arg = atoi(argv[4]);
    int bank2_arg = atoi(argv[5]);
    unbuffered_io();
    init_signal();
    init_bank();
    init_threads(fib_arg, fact_arg, bank1_arg, bank2_arg);
    start_threading();
}
