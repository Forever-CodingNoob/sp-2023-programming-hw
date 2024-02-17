#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define POST_LEN (FROM_LEN+CONTENT_LEN)
#define BUFFER_SIZE (RECORD_NUM*POST_LEN+16)


typedef struct{
    char from[FROM_LEN];
    char content[CONTENT_LEN];
} post;

typedef struct{
    uint32_t post_count;
    post postList[RECORD_NUM];
} pull_response;


typedef struct {
    char* ip; // server's ip
    unsigned short port; // server's port
    int conn_fd; // fd to talk with server
    char buf[BUFFER_SIZE]; // data sent by/to server
    size_t buf_len; // bytes used by buf
} client;



client cli;
size_t inputbufSize;
char* inputbuf; 

static void init_client(char** argv);

int max(int a, int b){ return (a<b)?b:a; };
void pull(pull_response* resP);
void post_req();
void release_write_lock();
ssize_t readInput(char* msg);
ssize_t read_all(int fd, void *buf, size_t size);

int main(int argc, char** argv){
    
    // Parse args.
    if(argc!=3){
        ERR_EXIT("usage: [ip] [port]");
    }

    setbuf(stdout, NULL);

    pull_response res;

    // Handling connection
    init_client(argv);
    fprintf(stderr, "connect to %s %d\n", cli.ip, cli.port);

    fprintf(stdout, "==============================\nWelcome to CSIE Bulletin board\n");
    pull(&res);
    //fprintf(stdout, "\n");
    
    inputbufSize=(size_t)max(5, max(FROM_LEN, CONTENT_LEN));
    inputbuf=(char*)malloc(inputbufSize); 
    int bytesRead;

    while(1){
        // TODO: handle user's input
        if((bytesRead=readInput("Please enter your command (post/pull/exit): "))<=0) continue;
        //fprintf(stdout, "%s", inputbuf);
        if(strcmp("pull", inputbuf)==0) pull(&res);
        else if(strcmp("post", inputbuf)==0) post_req();
        else if(strcmp("exit", inputbuf)==0) break;
    }
    free(inputbuf);
    return 0;
}

static void init_client(char** argv){
    
    cli.ip = argv[1];

    if(atoi(argv[2])==0 || atoi(argv[2])>65536){
        ERR_EXIT("Invalid port");
    }
    cli.port=(unsigned short)atoi(argv[2]);

    struct sockaddr_in servaddr;
    cli.conn_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cli.conn_fd<0){
        ERR_EXIT("socket");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(cli.port);

    if(inet_pton(AF_INET, cli.ip, &servaddr.sin_addr)<=0){
        ERR_EXIT("Invalid IP");
    }

    if(connect(cli.conn_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        ERR_EXIT("connect");
    }

    return;
}

void pull(pull_response* resP){
    char command = 0;
    if(write(cli.conn_fd, &command, 1)==-1){
        fprintf(stderr, "error sending pull request\n");
        return;
    }
    
    /*
    cli.buf_len = read(cli.conn_fd, cli.buf, BUFFER_SIZE);
    if(cli.buf_len<=0){
        fprintf(stderr, "error receiving data from server\n");
        return;
    }
    memcpy(resP, cli.buf, (cli.buf_len<sizeof(*resP)?cli.buf_len:sizeof(*resP)));
    resP->post_count = ntohl(resP->post_count);
    */
    cli.buf_len = read_all(cli.conn_fd, cli.buf, sizeof(resP->post_count));
    if(cli.buf_len<=0){
        fprintf(stderr, "error receiving data from server\n");
        return;
    }
    memcpy(resP, cli.buf, (cli.buf_len<sizeof(resP->post_count)?cli.buf_len:sizeof(resP->post_count)));
    resP->post_count = ntohl(resP->post_count);

    if(resP->post_count>0){
        cli.buf_len = read_all(cli.conn_fd, cli.buf, POST_LEN*resP->post_count);
        if(cli.buf_len<=0){
            fprintf(stderr, "error receiving data from server\n");
            return;
        }
        memcpy(resP->postList, cli.buf, (cli.buf_len<POST_LEN*resP->post_count)?cli.buf_len:POST_LEN*resP->post_count);
    }



    fprintf(stdout, "==============================");
    for(int i=0; i<resP->post_count;i++){
        resP->postList[i].from[FROM_LEN-1]='\0';
        resP->postList[i].content[CONTENT_LEN-1]='\0';
        fprintf(stdout, "\nFROM: %s\nCONTENT:\n%s", resP->postList[i].from, resP->postList[i].content);
    }
    fprintf(stdout, "\n==============================\n");
}

void post_req(){
    char command = 1;
    if(write(cli.conn_fd, &command, 1)==-1){
        fprintf(stderr, "error sending post request\n");
        return;
    }
    cli.buf_len = read(cli.conn_fd, cli.buf, BUFFER_SIZE);
    if(cli.buf_len<=0 || !(cli.buf[0]==0||cli.buf[0]==1)){
        fprintf(stderr, "error receiving data from server\n");
        return;
    }

    if(cli.buf[0]==0){
        fprintf(stdout, "[Error] Maximum posting limit exceeded\n");
        return;
    }
    
    // start posting data
    post P;
    memset(&P, 0, sizeof(P));
    if(readInput("FROM: ")<=1) return release_write_lock();
    strncpy(P.from, inputbuf, FROM_LEN-1);
    //P.from[FROM_LEN-1]='\0'
    if(readInput("CONTENT:\n")<=1) return release_write_lock();
    strncpy(P.content, inputbuf, CONTENT_LEN-1);
    //P.content[CONTENT_LEN-1]='\0'
    
    command = 2;
    cli.buf[0]=command;
    memcpy(cli.buf+1, &P, POST_LEN);
    cli.buf_len=1+POST_LEN;
    if(write(cli.conn_fd, cli.buf, cli.buf_len)==-1){
        fprintf(stderr, "error sending post data\n");
        release_write_lock();
        return;
    }
}

void release_write_lock(){
    char command = 3;
    if(write(cli.conn_fd, &command, 1)==-1){
        fprintf(stderr, "error sending release write lock request\n");
        return;
    }
}

ssize_t readInput(char* msg){
    if(msg!=NULL) fprintf(stdout, "%s", msg);
    ssize_t bytesRead = getline(&inputbuf, &inputbufSize, stdin);
    if(bytesRead==-1) fprintf(stderr, "error reading input\n");
    else if(bytesRead>0) inputbuf[bytesRead-1]='\0';
    return bytesRead;
}

ssize_t read_all(int fd, void *buf, size_t size){
    size_t total_bytes_received = 0;
    while (total_bytes_received < size) {
        ssize_t bytes_received = recv(fd, buf + total_bytes_received, size - total_bytes_received, MSG_WAITALL);
        if (bytes_received <= 0) return bytes_received;
        total_bytes_received += bytes_received;
    }
    return total_bytes_received;
}
