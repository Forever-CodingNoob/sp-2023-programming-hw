#include "hw1.h"
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define POST_LEN (FROM_LEN+CONTENT_LEN)
#define BUFFER_SIZE (RECORD_NUM*POST_LEN+16)

#define read_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
#define write_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define unlock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct{
    char from[FROM_LEN];
    char content[CONTENT_LEN];
} post;

typedef struct{
    char command;
    post req_post;
} request;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    request req;
    //char buf[BUFFER_SIZE];  // data sent by/to client
    //size_t buf_len;  // bytes used by buf
    int post_id;
} client;


server svr;  // server
client* clientSockets = NULL;  // point to a list of clients
//int maxfd;  // size of open file descriptor table, size of request list
int maxClients; // size of request list
bool write_locked[RECORD_NUM];
//bool first_log;

// initailize a server, exit for error
static void init_server(unsigned short port);

// initailize a client instance
static void init_client(client* cliP);

// free resources used by a client instance
static void free_client(client* cliP, int file_fd);

void release_write_lock(client* cliP, int file_fd);

// reference: sp04-adv-io.pdf p.46
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);

void post_req(client* cliP, int file_fd, int* lastP, char* buf, int* buf_lenP);
void post_content(client* cliP, int file_fd);
void pull(int file_fd, char* buf, int* buf_lenP);

void trunc_file(int file_fd);

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        ERR_EXIT("usage: [port]");
        //exit(1);
    }
    
    setbuf(stdout, NULL);

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[BUFFER_SIZE];
    int buf_len;
    fd_set readSet;
    int last=0;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxClients);
    
    if ((file_fd=open(RECORD_PATH, O_RDWR|O_CREAT, 00644))==-1){
        ERR_EXIT("cannot open RECORD_PATH");
    }
    trunc_file(file_fd);
    for(int i=0; i<RECORD_NUM; i++) write_locked[i]=false;
    //first_log=true;

    while (1) {
        // TODO: Add IO multiplexing
        FD_ZERO(&readSet);
        FD_SET(svr.listen_fd, &readSet);

        int maxfd=svr.listen_fd;
        for(int i=0;i<maxClients;i++){
            if(clientSockets[i].conn_fd>=0){
                FD_SET(clientSockets[i].conn_fd, &readSet);
                if(clientSockets[i].conn_fd > maxfd) maxfd=clientSockets[i].conn_fd;
            } 
        }

        struct timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
        int readySockets = select(maxfd+1, &readSet, NULL, NULL, &timeout);

        if (readySockets<=0){
            if(readySockets<0) fprintf(stderr, "error in select\n");
            //else fprintf(stderr, "select timeout reached\n");
            continue;
        } 

        // Check new connection
        if(FD_ISSET(svr.listen_fd, &readSet)){
            clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    fprintf(stderr, "out of file descriptor table ...\n");
                    continue;
                }
                ERR_EXIT("accept");
            }
            int j;
            for(j=0;j<maxClients;j++){
                if(clientSockets[j].conn_fd==-1){
                    clientSockets[j].conn_fd = conn_fd;
                    strcpy(clientSockets[j].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, clientSockets[j].host);
                    break;
                }
            }
            if(j==maxClients){
                close(conn_fd);
                fprintf(stderr, "maximum number of connected client reached!\n");
            }        
        }

        // TODO: handle requests from clients
        for(int i=0; i<maxClients; i++){
            if(clientSockets[i].conn_fd==-1 || FD_ISSET(clientSockets[i].conn_fd, &readSet)==0) continue;
            //fprintf(stderr, "i==%d, FD_ISSET(%d)==%d\n", i, clientSockets[i].conn_fd, FD_ISSET(clientSockets[i].conn_fd, &readSet));
            /* 
            buf_len = read(clientSockets[i].conn_fd, buf, BUFFER_SIZE);
            if(buf_len<=0){
                if(buf_len==0) fprintf(stderr, "client disconnected\n");
                else fprintf(stderr, "error reading data\n");
                close(clientSockets[i].conn_fd);
                free_client(&clientSockets[i], file_fd);
                continue;
            }
            memcpy(&clientSockets[i].req, buf, (buf_len<sizeof(request))?buf_len:sizeof(request));
            */
            
            buf_len = recv(clientSockets[i].conn_fd, buf, 1, 0);
            if(buf_len<=0){
                if(buf_len==0) fprintf(stderr, "client disconnected\n");
                else fprintf(stderr, "error reading data\n");
                close(clientSockets[i].conn_fd);
                free_client(&clientSockets[i], file_fd);
                continue;
            }
            memcpy(&clientSockets[i].req, buf, 1);
            
            buf_len=0;
            switch (clientSockets[i].req.command){
                case 0:
                    //pull
                    pull(file_fd, buf, &buf_len);
                    break;
                case 1:
                    //post request
                    post_req(&clientSockets[i], file_fd, &last, buf, &buf_len);
                    break;
                case 2:
                    //send post data
                    
                    buf_len = recv(clientSockets[i].conn_fd, buf, POST_LEN, MSG_WAITALL);
                    if(buf_len<=0){
                        if(buf_len==0) fprintf(stderr, "client disconnected\n");
                        else fprintf(stderr, "error reading data\n");
                        close(clientSockets[i].conn_fd);
                        free_client(&clientSockets[i], file_fd);
                        continue;
                    }
                    memcpy(&clientSockets[i].req.req_post, buf, (buf_len<sizeof(post))?buf_len:sizeof(post));
                    buf_len=0;
                    
                    post_content(&clientSockets[i], file_fd);
                    break;
                case 3:
                    //release write lock request
                    release_write_lock(&clientSockets[i], file_fd);
                    break;
            }
            if(buf_len>0){
                if(write(clientSockets[i].conn_fd, buf, buf_len)==-1){
                    fprintf(stderr, "error sending data (command == %d)\n", clientSockets[i].req.command);
                    //close(clientSockets[i].conn_fd);
                    //free_client(&clientSockets[i], file_fd);
                }
            }
        }
    }
    free(clientSockets);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_client(client* cliP) {
    cliP->conn_fd = -1;
    cliP->post_id = -1;
}

static void free_client(client* cliP, int file_fd) {
    release_write_lock(cliP, file_fd);
    init_client(cliP);
}

void release_write_lock(client* cliP, int file_fd){
    if(write_locked[cliP->post_id]){
        unlock(file_fd, POST_LEN*cliP->post_id, SEEK_SET, POST_LEN);
        write_locked[cliP->post_id]=false;
        cliP->post_id=-1;
    }
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    //maxfd = getdtablesize();
    maxClients = MAX_CLIENTS;
    clientSockets = (client*) malloc(sizeof(client) * maxClients);
    if (clientSockets == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxClients; i++) {
        init_client(&clientSockets[i]);
    }
    //requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    //strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len){
    struct flock lock;
    lock.l_type = type; /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = offset; /* byte offset, relative to l_whence */
    lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = len; /* #bytes (0 means to EOF) */
    return(fcntl(fd, cmd, &lock));
}

void post_req(client* cliP, int file_fd, int* lastP, char* buf, int* buf_lenP){
    int post_id;
    for(int i=0;i<RECORD_NUM;i++){
        post_id=(*lastP+i)%RECORD_NUM;
        if(!write_locked[post_id] && write_lock(file_fd, POST_LEN*post_id, SEEK_SET, POST_LEN)!=-1){
            cliP->post_id = post_id;
            *lastP = (post_id+1)%RECORD_NUM;
            buf[0]=1;
            *buf_lenP = 1;
            write_locked[post_id]=true;
            fprintf(stderr, "get write lock for post %d\n", post_id);
            return;
        }
    }
    // Maximum posting limit exceeded
    buf[0]=0;
    *buf_lenP = 1;
}

void post_content(client* cliP, int file_fd){
    if(cliP->post_id==-1){
        fprintf(stderr, "writing area is not locked\n");
        return;
    }
    cliP->req.req_post.from[FROM_LEN-1]=cliP->req.req_post.content[CONTENT_LEN-1]='\0';
    
    // pad \0
    size_t len = strlen(cliP->req.req_post.from);
    memset(cliP->req.req_post.from+len, 0, FROM_LEN-len);
    len = strlen(cliP->req.req_post.content);
    memset(cliP->req.req_post.content+len, 0, CONTENT_LEN-len);


    if(pwrite(file_fd, &(cliP->req.req_post), POST_LEN, POST_LEN*cliP->post_id)==-1) 
        fprintf(stderr, "error writing post\n");

    release_write_lock(cliP, file_fd);    
    
    fprintf(stdout, "[Log] Receive post from %s\n", cliP->req.req_post.from);
    //first_log=false;
}

void pull(int file_fd, char* buf, int* buf_lenP){
    uint32_t unlocked_count=0, locked_count=0, locked_by_self=0;
    int offset=sizeof(unlocked_count);
    int n;
    post p;
    //fprintf(stderr, "start pulling...\n");
    for(int i=0;i<RECORD_NUM;i++){
        if(write_locked[i] || read_lock(file_fd, POST_LEN*i, SEEK_SET, POST_LEN)==-1){
            locked_count++;
            if(write_locked[i]) locked_by_self++;
            continue;
        }
        //fprintf(stderr, "get readlock for post %d\n", i);
        n=pread(file_fd, &p, POST_LEN, POST_LEN*i);
        // n==0 iff EOF
        if(n==-1) fprintf(stderr, "error reading post %d\n", i);
        else if(n>0 && !(p.from[0]=='\0' && p.content[0]=='\0')){
            strncpy(buf+offset+POST_LEN*unlocked_count, p.from, FROM_LEN);
            strncpy(buf+offset+POST_LEN*unlocked_count+FROM_LEN, p.content, CONTENT_LEN);
            //fprintf(stderr, "content: %s\n", p.content);
            unlocked_count++;
        }        
        unlock(file_fd, POST_LEN*i, SEEK_SET, POST_LEN);
        //if(n==0) break;
    }
    if(locked_count>0){
        fprintf(stdout, "[Warning] Try to access locked post - %u\n", locked_count);
        //fprintf(stdout, "[Warning] self locked: %d, locked by others: %d\n", locked_by_self, locked_count-locked_by_self);
        //first_log=false;
    }
    *buf_lenP = offset+POST_LEN*unlocked_count;
    unlocked_count=htonl(unlocked_count);
    memcpy(buf, &unlocked_count, offset);   
}

void trunc_file(int file_fd){
    // try to truncate the file with 0
    if(ftruncate(file_fd, POST_LEN*RECORD_NUM)==-1) 
        fprintf(stderr, "error truncating the bulletinboard\n");
}
