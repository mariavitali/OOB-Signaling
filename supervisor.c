
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "utillib.h"


volatile sig_atomic_t sigIntCounter = 0;    //number of SIGINT received
volatile sig_atomic_t closed = 0;
static pipefd * pipeServer;                 //array to save connection pipe to all servers
static pid_t * pidServer;                   //array to save servers' pid
static infotable table;                     //info list to save best secret estimates
static int k;                               //number of servers

void manageServer(int k);
static infotable updateInfotable(infotable t, info newinfo);
static void sigManager(int signum);
static void setHandler();
void closeSupervisor();



int main(int argc, char* argv[]){
    //initialize all variables
    pipeServer = NULL;
    pidServer = NULL;          
    table = NULL;

    //check for correct main arguments
    //k  ->  number of servers to be activated (int)
    if(argc != 2){
        fprintf(stderr, "Use: %s   (int) k\n", argv[0]);
        exit(-1);
    }

    //convert to int
    errno = 0;
    k = strtol(argv[1], NULL, 10);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    //set SIGINT && SIGALRM handler
    setHandler();

    //start supervisor activity
    fprintf(stdout, "SUPERVISOR STARTING %d\n", k);
    manageServer(k);

    //close supervisor and servers: cleanup
    closeSupervisor();
    return 0;
}


void manageServer(int k){

    pipeServer = malloc(k * sizeof(pipefd));
    pidServer = malloc(k * sizeof(pid_t));

    //create and manage connection with k processes 
    for(int i = 0; i < k; i++){
        //create pipe to communicate with child i
        SYSCALL(pipe(pipeServer[i].fd), "creating pipe");

        int pidChild;

        SYSCALL((pidChild = fork()), "fork");
        if(pidChild == 0){
            //child process


            //redirect child stdout and stderr  
            //int fd = open("./outserver.log", O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);            
            int fd = open("./outserver.log", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);


            dup2(fd, 1);   // make stdout go to file
            dup2(fd, 2);   // make stderr go to file

            close(fd);

            //close reading pipe
            SYSCALL(close(pipeServer[i].fd[0]), "closing reading pipe");

            execl("./server", "server", &i, &pipeServer[i].fd[1], NULL);
            //if execl returns => error
            perror("execl");
            exit(errno);
        }
        else{       //parent process, pidChild > 0
            pidServer[i] = pidChild;

            //close writing pipe
            SYSCALL(close(pipeServer[i].fd[1]), "closing writing pipe");
            
            //set non-blocking read
            int flags = fcntl(pipeServer[i].fd[0], F_GETFL, 0);
            fcntl(pipeServer[i].fd[0], F_SETFL, flags | O_NONBLOCK);

        }
    }

    RANDOMIZE(getpid());

    while(!closed){
        //check read random pipe
        int randomIndex = RANDOM(k);
        int nread = 0;;
        info received;
        received.client_id = 0;
        received.estimatedSecret = -1;
        received.nServer = 0;
        received.next = NULL;

        //read from pipe
        nread = read(pipeServer[randomIndex].fd[0], &received, sizeof(info));
        if(nread > 0){
            fprintf(stdout, "SUPERVISOR ESTIMATE %d FOR %lx FROM %d\n", received.estimatedSecret, received.client_id, randomIndex+1);
            //update info table
            table = updateInfotable(table, received);
        }
    }  

}


static infotable updateInfotable(infotable t, info newinfo){
    //info list is empty, add first element
    if(t == NULL){
        info* newelem = malloc(sizeof(info));
        newelem->client_id = newinfo.client_id;
        newelem->estimatedSecret = newinfo.estimatedSecret;
        newelem->nServer = 1;
        newelem->next = NULL;
        t = newelem;
        return t;
    }
    
    //infolist is not empty, search for newinfo.client_id
    int exists = 0;
    info* curr = t;
    if(!exists && (curr!=NULL)){
        if(curr->client_id == newinfo.client_id){
            exists = 1;
        }
        else{
            curr = curr->next;
        }
    }

    //not found: new clientid, add element to the list
    if(exists == 0){
        info *newelem = malloc(sizeof(info));
        newelem->client_id = newinfo.client_id;
        newelem->estimatedSecret = newinfo.estimatedSecret;
        newelem->nServer = 1;
        newelem->next = t;
        t = newelem;
    }
    //element already in the list: update estimate (if needed)
    else{
        curr->nServer++;
        if(curr->estimatedSecret < 0){          //had not received any estimate before
            curr->estimatedSecret = newinfo.estimatedSecret;
        }
        else if(newinfo.estimatedSecret < curr->estimatedSecret){
            curr->estimatedSecret = newinfo.estimatedSecret;
        }
    }
    return t;
}


//handler SIGINT e SIGALRM
static void sigManager(int signum){
    if(signum == SIGINT){
        sigIntCounter++;
        if(sigIntCounter == 1){
            alarm(1);       //start 1 sec timer
        }
        else if(sigIntCounter == 2){        //gets here only if 1 second time has not expired
            closed=1;            
        }
    }

    if(signum == SIGALRM){
        
        if(sigIntCounter == 1){        //not double SIGINT within 1 sec
            printInfotable(table, stderr);  //print info table on stderr
            sigIntCounter = 0;
        }
    }
}


//set signal handlers
static void setHandler(){
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigManager;
    SYSCALL((sigaction(SIGINT, &action, NULL)), "sigaction");
    SYSCALL((sigaction(SIGALRM, &action, NULL)), "sigaction");
}


//close supervisor, notify servers to terminate, free memory
void closeSupervisor(){

    printInfotable(table, stdout);          //print info table on stdout
    fprintf(stdout, "SUPERVISOR EXITING\n");

    for(int i=0; i<k; i++){
        SYSCALL(kill(pidServer[i], SIGTERM), "kill");
        SYSCALL(close(pipeServer[i].fd[0]), "close pipe");
    }

    infotable curr = table;
    while(curr != NULL){
        table = curr->next;
        free(curr);
        curr = table;
    }

    free(pidServer);
    free(pipeServer);
    fflush(NULL);
    exit(0);
}
