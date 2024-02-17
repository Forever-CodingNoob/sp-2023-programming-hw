#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "util.h"

#define ERR_SYS(s) do { perror(s); exit(1); } while(0)
#define ERR_EXIT(a) do { fprintf(stderr, a); fputc('\n', stderr); exit(1); } while(0)

#define INPUT_BUFFSIZE MAX_CMD_LEN
#define MAX_ARGC 3

typedef struct child{
    pid_t pid;
    int readfd;
    int writefd;
    struct child* next;
} Child, *ChildList;

typedef struct{
    char status;
    unsigned char number_of_descendents;
} KillMsg;


static unsigned long secret;
static char service_name[MAX_SERVICE_NAME_LEN];

char* inputbuf=NULL;
char buf[INPUT_BUFFSIZE];
size_t inputbufSize=INPUT_BUFFSIZE; 
char* input_argv[MAX_ARGC];
ChildList childlist=NULL;
int childcount=0;





static inline bool is_manager() {
    return strcmp(service_name, "Manager") == 0;
}

void print_not_exist(char *service_name) {
    printf("%s doesn't exist\n", service_name);
}

void print_receive_command(char *service_name, char *cmd) {
    printf("%s has received %s\n", service_name, cmd);
}

void print_spawn(char *parent_name, char *child_name) {
    printf("%s has spawned a new service %s\n", parent_name, child_name);
}

void print_kill(char *target_name, int decendents_num) {
    printf("%s and %d child services are killed\n", target_name, decendents_num);
}

void print_acquire_secret(char *service_a, char *service_b, unsigned long secret) {
    printf("%s has acquired a new secret from %s, value: %lu\n", service_a, service_b, secret);
}

void print_exchange(char *service_a, char *service_b) {
    printf("%s and %s have exchanged their secrets\n", service_a, service_b);
}


int readInput(int fd);
void check_argc(int _argc, int lowerbound){
    if(_argc<lowerbound) ERR_EXIT("received command arguments are insufficient");
}

void spawn(char* parent, char* child);
void spawn_child(char* child);

void find_kill(char* name);
void let_child_suicide(Child* child);
void terminate();

void exchange(char* service_a, char* service_b, const int found);
void fifoname(char* s, char* from, char* to);


int main(int argc, char *argv[]) {
    pid_t pid = getpid();        

    if (argc != 2) {
        fprintf(stderr, "Usage: ./service [service_name]\n");
        return 0;
    }

    srand(pid);
    secret = rand();
    /* 
     * prevent buffered I/O
     * equivalent to fflush() after each stdout
     */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    strncpy(service_name, argv[1], MAX_SERVICE_NAME_LEN);

    printf("%s has been spawned, pid: %d, secret: %lu\n", service_name, pid, secret);

    inputbuf=(char*)malloc(inputbufSize);
    /*
    int parent_read_fd, parent_write_fd;
    if(is_manager()) parent_read_fd=STDIN_FILENO, parent_write_fd=STDOUT_FILENO;
    else parent_read_fd=3, parent_write_fd=4;
    */

    if(!is_manager()){
        // send spawn msg back to parent
        char c=1;
        if(write(PARENT_WRITE_FD, &c, 1)!=1) ERR_SYS("write error");
    }

    while(1){
        int input_argc=readInput(is_manager()?STDIN_FILENO:PARENT_READ_FD);
        check_argc(input_argc, 1);
        
        char* command=input_argv[0];
        if(strcmp(command,"spawn")==0){
            check_argc(input_argc, 3);
            sprintf(buf, "spawn %s %s", input_argv[1], input_argv[2]);
            print_receive_command(service_name, buf);
            spawn(input_argv[1], input_argv[2]);
        }else if(strcmp(command, "kill")==0){
            check_argc(input_argc, 2);
            sprintf(buf, "kill %s", input_argv[1]);
            print_receive_command(service_name, buf);
            find_kill(input_argv[1]);
        }else if(strcmp(command, "killall")==0 && !is_manager()){
            terminate();
        }else if(strcmp(command, "exchange")==0){
            check_argc(input_argc, 3);
            sprintf(buf, "exchange %s %s", input_argv[1], input_argv[2]);
            print_receive_command(service_name, buf);
            exchange(input_argv[1], input_argv[2], 0);
        }else if(strcmp(command, "exchange_foundone")==0 && !is_manager()){
            check_argc(input_argc, 3);
            sprintf(buf, "exchange %s %s", input_argv[1], input_argv[2]);
            print_receive_command(service_name, buf);
            exchange(input_argv[1], input_argv[2], 1);
        }else ERR_EXIT("invalid command");
    }
    return 0;
}

void spawn(char* parent, char* child){
    if(strcmp(service_name, parent)==0){
        spawn_child(child);
        if(is_manager()) print_spawn(parent, child);
        else{
            char msg=1;
            if(write(PARENT_WRITE_FD, &msg, 1)!=1) ERR_SYS("error telling parent spawn is complete");
        }
    }else{
        Child* p=childlist;
        char msg;
        while(p!=NULL){
            sprintf(buf, "spawn %s %s", parent, child);
            if(write(p->writefd, buf, strlen(buf))!=strlen(buf))  ERR_SYS("error passing spawn to child");
            
            // wait for child
            if(read(p->readfd, &msg, 1)!=1) ERR_SYS("error receiving spawn status from child");
            if(msg==1) break;
            p=p->next;
        }
        if(p==NULL){
            // spawn failed since parent was not found
            if(is_manager()){
                print_not_exist(parent);
            }else{
                msg=0;
                if(write(PARENT_WRITE_FD, &msg, 1)!=1) ERR_SYS("error telling parent spawn failed");
            }
        }else{
            // spawn succeeded
            if(is_manager()){
                print_spawn(parent, child);
            }else{
                msg=1;
                if(write(PARENT_WRITE_FD, &msg, 1)!=1) ERR_SYS("error telling parent spawn succeeded");
            }
        }
    }
}

void spawn_child(char* child){ 
    Child* p=(Child*)malloc(sizeof(Child));
    p->next=NULL;
        
    int p2cfd[2], c2pfd[2]; //parent-to-child, child-to-parent
    if(pipe2(p2cfd, O_CLOEXEC)<0||pipe2(c2pfd, O_CLOEXEC)<0) ERR_SYS("pipe error");
    //fprintf(stderr, "%d, %d, %d, %d\n", p2cfd[0], p2cfd[1], c2pfd[0], c2pfd[1]);

    p->readfd=c2pfd[0];
    p->writefd=p2cfd[1];

    pid_t pid;
    if((pid=fork())<0) ERR_SYS("fork error");
    else if(pid==0){
        // child process
        // DO NOT DO THIS: if(!is_manager()) close(PARENT_READ_FD), close(PARENT_WRITE_FD);
                        
        int tmp_write_fd=dup(c2pfd[1]), tmp_read_fd=dup(p2cfd[0]);
        close(c2pfd[1]), close(p2cfd[0]), close(c2pfd[0]), close(p2cfd[1]);
        dup2(tmp_write_fd, PARENT_WRITE_FD);
        dup2(tmp_read_fd, PARENT_READ_FD);
        close(tmp_read_fd), close(tmp_write_fd);

        //fprintf(stderr, "%d, %d, %d, %d,,, %d, %d\n", p2cfd[0], p2cfd[1], c2pfd[0], c2pfd[1], tmp_read_fd, tmp_write_fd);

        
        /*
        int flag=fcntl(PARENT_READ_FD, F_GETFD);
        fcntl(PARENT_READ_FD, F_SETFD, flag&(~FD_CLOEXEC));
        flag=fcntl(PARENT_WRITE_FD, F_GETFD);
        fcntl(PARENT_WRITE_FD, F_SETFD, flag&(~FD_CLOEXEC));
        */

        if(execl("./service", "service", child, NULL)<0) ERR_SYS("exec error");
    }else{
        // parent process
        close(c2pfd[1]), close(p2cfd[0]);
        p->pid=pid;
        
        char c;
        if(read(p->readfd, &c, 1)!=1 || c!=1) ERR_SYS("read error");
    }

    if(childlist==NULL) childlist=p;
    else{
        Child* head=childlist;
        while(head->next!=NULL) head=head->next;
        head->next=p;
    }
    childcount++;
}


void find_kill(char* name){
    if(strcmp(name, service_name)==0) terminate();

    Child* head=childlist, *prev=NULL;
    KillMsg killmsg;
    unsigned char number_of_descendents;
    while(head!=NULL){
        sprintf(buf, "kill %s", name);
        if(write(head->writefd, buf, strlen(buf))!=strlen(buf))  ERR_SYS("error passing kill to child");
        
        // wait for child
        if(read(head->readfd, &killmsg, sizeof(killmsg))!=sizeof(killmsg)) ERR_SYS("error receiving kill status from child");
        //fprintf(stderr, "kill.status==%d\n", killmsg.status);
        if(killmsg.status==1){
            // kill succeeded and "name" is not child, then return
            number_of_descendents=killmsg.number_of_descendents;
            break;
        }else if(killmsg.status==2){
            // victim is child
            if(prev!=NULL) prev->next=head->next;
            else childlist=head->next;
            number_of_descendents = killmsg.number_of_descendents;
            
            let_child_suicide(head);

            break;
        }else if(killmsg.status!=0) ERR_EXIT("kill status invalid");
        prev=head;
        head=head->next;
    }
    if(head==NULL){
        // not exist
        if(is_manager()){
            print_not_exist(name);
        }else{
            killmsg.status=0;
            if(write(PARENT_WRITE_FD, &killmsg, sizeof(killmsg))!=sizeof(killmsg)) 
                ERR_SYS("error telling parent kill failed");
        }
    }else{
        // kill succeeded 
        if(is_manager()) print_kill(name, number_of_descendents);
        else{
            killmsg.status=1;
            killmsg.number_of_descendents=number_of_descendents;
            if(write(PARENT_WRITE_FD, &killmsg, sizeof(killmsg))!=sizeof(killmsg)) 
                ERR_SYS("error telling parent kill succeeded");
        }
    }

}

void let_child_suicide(Child* child){
    /* You must send kill or killall command to child first! */
    KillMsg killmsg;
    // tell child to kill itself (yes, suicide)
    killmsg.status=3;
    //fprintf(stderr, "not done telling suicide\n");
    if(write(child->writefd, &killmsg, sizeof(killmsg))!=sizeof(killmsg)) ERR_SYS("error telling child to suicide");
    
    //fprintf(stderr, "done telling suicide\n");
    // wait for child to terminate
    if(waitpid(child->pid, NULL, 0)<0) ERR_SYS("waitpid error");

    close(child->readfd), close(child->writefd);
    free(child);
    childcount--;
}

void terminate(){
    // kill all childs first
    //fprintf(stderr, "terminating %s...\n", service_name);
    Child* head=childlist, *next;
    KillMsg killmsg;
    unsigned char number_of_descendents=0;
    while(head!=NULL){
        next=head->next;
        if(write(head->writefd, "killall", 7)!=7) ERR_SYS("error passing killall to child");
        
        // wait for child
        if(read(head->readfd, &killmsg, sizeof(killmsg))!=sizeof(killmsg)) ERR_SYS("error receiving kill status from child");
        if(killmsg.status!=2) ERR_EXIT("kill status invalid");
        number_of_descendents+=killmsg.number_of_descendents+1;
        
        let_child_suicide(head);

        head=next;
    }
    
    if(is_manager()){
        print_kill(service_name, number_of_descendents);
    }else{
        //int n;
        killmsg.number_of_descendents=number_of_descendents;
        killmsg.status=2;
        if((write(PARENT_WRITE_FD, &killmsg, sizeof(killmsg)))!=sizeof(killmsg)) ERR_SYS("error sending number of decendents to parent");
        
        if((read(PARENT_READ_FD, &killmsg, sizeof(killmsg)))!=sizeof(killmsg)) ERR_SYS("error waiting for parent to agree suicide");
        if(killmsg.status!=3) ERR_EXIT("kill status invalid");
    
        close(PARENT_READ_FD), close(PARENT_WRITE_FD);
    }


    free(inputbuf);

    exit(0);
}

void exchange(char* service_a, char* service_b, const int found){
    int found_here=0;
    char* peer=NULL;
    int to_wait[2]={-1,-1};
    bool is_service_a=false;
    if(strcmp(service_name, service_a)==0) peer=service_b, found_here++, is_service_a=true;
    else if(strcmp(service_name, service_b)==0) peer=service_a, found_here++;

    Child* head=childlist;
    unsigned char msg;
    while(head!=NULL && found+found_here<2){
        sprintf(buf, "%s %s %s", (found+found_here==1)?"exchange_foundone":"exchange",service_a, service_b);
        if(write(head->writefd, buf, strlen(buf))!=strlen(buf))  ERR_SYS("error passing exchange to child");
            
        // wait for child
        if(read(head->readfd, &msg, 1)!=1) ERR_SYS("error receiving exchange status from child");
        if(msg==2){
            to_wait[0]=head->readfd;
            found_here=2;
        }
        else if(msg==1) to_wait[found_here++]=head->readfd;
        head=head->next;
    }
    if(is_manager()){ 
        if(found+found_here<2) fprintf(stderr, "%s or %s does not exist", service_a, service_b); 
    }else{
        msg=(unsigned char)(found_here);
        if(write(PARENT_WRITE_FD, &msg, 1)!=1) ERR_SYS("error telling parent exchange found count");
    }
    
    if(found_here==0) return;
    if(peer!=NULL){
        char read_fifo[MAX_FIFO_NAME_LEN], write_fifo[MAX_FIFO_NAME_LEN];
        fifoname(read_fifo, peer, service_name), fifoname(write_fifo, service_name, peer);
        
        if(mkfifo(read_fifo, 0666)==-1 && errno!=EEXIST) ERR_SYS("mkfifo error"); 
        if(mkfifo(write_fifo, 0666)==-1 && errno!=EEXIST) ERR_SYS("mkfifo error"); 

        int read_fd, write_fd;
        unsigned long new_secret;

        if(is_service_a){
            if((read_fd=open(read_fifo, O_RDONLY))<0) ERR_SYS("open fifo error");
            if(read(read_fd, &new_secret, sizeof(new_secret))!=sizeof(new_secret)) ERR_SYS("error reading secret of peer");
            print_acquire_secret(service_name, peer, new_secret);
            close(read_fd);
            if(unlink(read_fifo)<0) ERR_SYS("unlink fifo error");
            
            if((write_fd=open(write_fifo, O_WRONLY))<0) ERR_SYS("open fifo error");
            if(write(write_fd, &secret, sizeof(secret))!=sizeof(secret)) ERR_SYS("error sending secret of self");
            close(write_fd);
        }else{
            if((write_fd=open(write_fifo, O_WRONLY))<0) ERR_SYS("open fifo error");
            if(write(write_fd, &secret, sizeof(secret))!=sizeof(secret)) ERR_SYS("error sending secret of self");
            close(write_fd);
            
            if((read_fd=open(read_fifo, O_RDONLY))<0) ERR_SYS("open fifo error");
            if(read(read_fd, &new_secret, sizeof(new_secret))!=sizeof(new_secret)) ERR_SYS("error reading secret of peer");
            print_acquire_secret(service_name, peer, new_secret);
            close(read_fd);
            if(unlink(read_fifo)<0) ERR_SYS("unlink fifo error");
        }
        secret=new_secret;
    }
    
    char c=0;
    for(int j=0;j<=1;j++){
        if(to_wait[j]!=-1){
            if(read(to_wait[j], &c, 1)!=1) ERR_SYS("error waiting for child to finish doing fifo");
            if(c!=0) ERR_EXIT("invalid fifo status");
        }
    }
    if(is_manager()) print_exchange(service_a, service_b);
    else if(write(PARENT_WRITE_FD, &c, 1)!=1)  ERR_SYS("error telling parent fifo is finished");
}

void fifoname(char* s, char* from, char* to){
    sprintf(s, "%s_to_%s.fifo", from, to);
}


int readInput(int fd){
    /* fd==0: read from stdin */
    ssize_t bytesRead;
    if(fd==STDIN_FILENO){
        bytesRead = getline(&inputbuf, &inputbufSize, stdin);
        if(bytesRead==-1) ERR_SYS("error reading input from stdin");
        else if(bytesRead>0) inputbuf[bytesRead-1]='\0';
    }else{
        bytesRead = read(fd, inputbuf, inputbufSize);
        if(bytesRead<=0) ERR_SYS("error reading from pipe");
        inputbuf[bytesRead]='\0';
    }
    
    
    int argc=0;
    char* ptr=strtok(inputbuf, " ");
    while(ptr!=NULL && argc<MAX_ARGC){
        input_argv[argc++]=ptr;
        ptr=strtok(NULL, " ");
    }
    return argc;
}
