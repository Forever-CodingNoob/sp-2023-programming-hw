#define PARENT_READ_FD 3
#define PARENT_WRITE_FD 4
#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 64
#define MAX_SERVICE_NAME_LEN 16
#define MAX_CMD_LEN 128
#include <sys/types.h>
typedef struct {
    pid_t pid;
    int read_fd;
    int write_fd;
    char name[MAX_SERVICE_NAME_LEN];
} service;

