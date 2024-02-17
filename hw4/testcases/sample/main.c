#include "my_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *func0(void *args) {
  printf("func 0: %d\n", *((int *)args));
  fflush(stdout);
  return NULL;
}
void *func1(void *args) {
  printf("func 1: %d\n", *((int *)args));
  fflush(stdout);
  return NULL;
}
#define N 2
#define M 3
int main() {
  tpool *pool = tpool_init(N);
  void *(*funcs[M])(void *) = {&func0, &func1, &func0};
  int *arg = malloc(M * sizeof(int));
  for (int i = 0; i < M; i++) {
    arg[i] = i;
    tpool_add(pool, funcs[i], (void *)&arg[i]);
  }
  tpool_wait(pool);
  tpool_destroy(pool);
  free(arg);
}