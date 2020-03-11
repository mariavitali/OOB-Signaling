
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "utillib.h"


volatile sig_atomic_t sigIntCounter = 0;    //number of SIGINT received
static pipefd * pipeServer;            //array to save connection pipe to all servers
static pid_t * pidServer;              //array to save servers' pid
static infotable table;                //info list to save best secret estimates
static int k;                          //number of servers

void manageServer(int k);
static infotable updateInfotable(infotable t, info newinfo);
static void sigIntManager(int signum);
static void setHandler();
void closeSupervisor();



int main(int argc, char* argv[]){
          
    table = NULL;

    /*  check for correct main arguments
        k  ->  number of servers to be activated (int)  */
    if(argc != 2){
        fprintf(stderr, "Use: %s   (int) k\n", argv[0]);
        exit(-1);
    }

    errno = 0;
    k = strtol(argv[1], NULL, 10);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    //set SIGINT && SIGALRM handler
    setHandler();

    //supervisor's stdout redirection
    //????????????????????????????????????????

    fprintf(stdout, "SUPERVISOR STARTING %d\n", k);
    manageServer(k);


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
            //figlio

            //close reading pipe
            SYSCALL(close(pipeServer[i].fd[0]), "closing reading pipe");

            execl("./server", "server", &i, &pipeServer[i].fd[1], NULL);
            //if execl returns => error
            perror("execl");
            exit(errno);
        }
        else{
            //padre, pidChild > 0
            pidServer[i] = pidChild;

            //closing writing pipe
            SYSCALL(close(pipeServer[i].fd[1]), "closing writing pipe");
            
            //set non blocking read
            int flags = fcntl(pipeServer[i].fd[0], F_GETFL, 0);
            fcntl(pipeServer[i].fd[0], F_SETFL, flags | O_NONBLOCK);

            fprintf(stdout, "supervisor fatto. ora aspetto per la lettura\n");


        }
    }

    RANDOMIZE;

    while(1){
        //check read random pipe
        int randomIndex = RANDOM(k);
        int nread;
        info received;

        //read from pipe
        nread = read(pipeServer[randomIndex].fd[0], &received, sizeof(info));
        if(nread > 0){
            fprintf(stdout, "SUPERVISOR ESTIMATE\t%d\tFOR\t%lx\tFROM\t%d\n", received.estimatedSecret, received.client_id, randomIndex+1);
            //update info table
            table = updateInfotable(table, received);
        }
    }  

}


static infotable updateInfotable(infotable t, info newinfo){
    //info list is empty
    if(t == NULL){
        info* newelem = malloc(sizeof(info));
        newelem->client_id = newinfo.client_id;
        newelem->estimatedSecret = newinfo.estimatedSecret;
        newelem->nServer = 1;
        newelem->next = NULL;
        t = newelem;
        return t;
    }
    
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
        if(newinfo.estimatedSecret < curr->estimatedSecret){
            curr->estimatedSecret = newinfo.estimatedSecret;
        }
    }
    return t;
}



static void sigIntManager(int signum){
    if(signum == SIGINT){
        sigIntCounter++;
        if(sigIntCounter == 1){
            alarm(1);       //start 1 sec timer
        }
        else if(sigIntCounter == 2){        //gets here only if 1 second time has not expired
            printInfotable(table, stdout);          //print info table on stdout
            closeSupervisor();
        }
    }

    if(signum == SIGALRM){
        
        if(sigIntCounter == 1){        //not double SIGINT within 1 sec
            printInfotable(table, stderr);  //print info table on stderr
            sigIntCounter = 0;
        }
    }
}


/*set signal handlers*/
static void setHandler(){
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigIntManager;
    SYSCALL((sigaction(SIGINT, &action, NULL)), "sigaction");
    SYSCALL((sigaction(SIGALRM, &action, NULL)), "sigaction");
}


void closeSupervisor(){

    //devo ignorare gli altri segnali mentre spengo tutto????????????

    for(int i=0; i<k; i++){
        SYSCALL(kill(pidServer[i], SIGTERM), "kill");
        SYSCALL(close(pipeServer[i].fd[0]), "close pipe");
    }

    free(table);
    free(pidServer);
    free(pipeServer);
    _exit(0);
}
