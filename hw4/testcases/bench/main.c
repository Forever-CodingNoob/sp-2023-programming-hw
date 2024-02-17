#include "my_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SIZE 100
typedef long long LL;

void *Collatz(void *args) {
  volatile LL x = *(LL *)args;
  volatile LL cnt = 0;
  while (x != 1) {
    if (x & 1)
      x = x * 3 + 1;
    else
      x /= 2;
    cnt++;
    // try uncomment printf
    // printf("%lld\n", x);
  }
  // try uncomment printf
  // printf("%lld\n", cnt);
  return NULL;
}

void *waste_time(void *args){
    LL x = *(LL*)args;
    LL *p = malloc(SIZE * sizeof(LL));
    for(int i=0; i<SIZE; i++){
        p[i] = x+i;
        Collatz((void*)&p[i]);
    }
    free(p);
    return NULL;
}

//#define N 10
int N;
#define M 0x800000 
int main(int argc, char* argv[]) {
  if(argc<=1){
    fprintf(stderr, "arguments are NOT sufficient.\n");
    exit(1);
  }
  N=atoi(argv[1]);
  tpool *pool = tpool_init(N);
  LL *arg = malloc(M * sizeof(LL));
  for (int i = 0; i < M; i++) {
    arg[i] = 0x10000000ll + i;
    //tpool_add(pool, Collatz, (void *)&arg[i]);
    tpool_add(pool, waste_time, (void *)&arg[i]);
  }
  tpool_wait(pool);
  tpool_destroy(pool);
  free(arg);
}
