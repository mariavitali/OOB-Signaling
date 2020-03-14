#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "utillib.h"
#include "utilconn.h"


//global variables
int servernum;
int pipess;              //pipe server-supervisor
char* socketname;
est_t* connectedClients; //data structure to keep track of clients' secret estimates
static int nClients;

static volatile sig_atomic_t terminate=0;


static void sighandler(int useless);
void executeServer();
int estimate(int fdClient);
int updatemax(fd_set set, int fdmax);
int member(uint64_t id, est_t* list);
void cleanup();



int main(int argc, char* argv[]){

    //atexit pulizia
    atexit(cleanup);

    //check main arguments
    if(argc < 3){
        fprintf(stderr, "%s must have 3 arguments: %s id_server_number  fd_pipe_server-supervisor\n", argv[0], argv[0]);
        exit(-1);
    }

    //set signal handler
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sighandler;
    SYSCALL((sigaction(SIGTERM, &action, NULL)), "sigaction");

    //ignore SIGPIPE and SIGINT
    struct sigaction ig;
    memset(&ig, 0, sizeof(ig));
    ig.sa_handler = SIG_IGN;
    SYSCALL((sigaction(SIGPIPE, &ig, NULL)), "sigaction ignore pipe");
    SYSCALL((sigaction(SIGINT, &ig, NULL)), "sigaction ignore sigint");

    //id number of server
    errno = 0;      
    servernum = (int)(*argv[1]) + 1;     //between 1 and k
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    //fd pipe server-supervisor
    pipess = (int)(*argv[2]);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    unlink(socketname);

    socketname = malloc(32 * sizeof(char));
    sprintf(socketname, "OOB-server-%d", servernum);
    fprintf(stdout, "servername %s\n", socketname);

    executeServer();

    return 0;
}


static void sighandler(int useless) {    
    terminate=1;
}



void executeServer(){

    //allocate space for 1st client
    connectedClients = malloc(sizeof(est_t));


/**************************************************SOCKET CONNECTI0N SET UP***************************************************************/

    int fdServerSocket;     // listening socket

    unlink(socketname);

    struct sockaddr_un address;
    strncpy(address.sun_path, socketname, UNIX_PATH_MAX);
    address.sun_family = DOMAIN;

    //create server socket (receiver)
    SYSCALL((fdServerSocket = socket(AF_UNIX, SOCK_STREAM, 0)), "creating server socket");

    //bind address to socket
    SYSCALL(bind(fdServerSocket, (struct sockaddr*)&address, sizeof(address)), "bind");

    //server is listening and waiting for client connection requests
    SYSCALL(listen(fdServerSocket, SOMAXCONN), "listen");
    fprintf(stdout, "SERVER %d ACTIVE\n", servernum);


/************************************************SELECT (FOR MULTITHREADING CONNECTION)*****************************************************/

    int fdmax = 0;  //max active fd
    int index;       //index to check select results
    fd_set set;     //active file descriptors set
    fd_set rdset;      //file descriptors to check for reading
    nClients = 0;   //number of clients currently connected

    if(fdServerSocket > fdmax)
        fdmax = fdServerSocket;
    fprintf(stdout, "fd max è %d\n", fdmax);
    
    FD_ZERO(&set);      //set to zero
    FD_ZERO(&rdset);
    FD_SET(fdServerSocket, &set);

    while(!terminate){
        //get select mask ready (at every new iteration, cause is modified by select)
        rdset = set;
        SYSCALL(select(fdmax+1, &rdset, NULL, NULL, NULL), "select");
        

        //check for new requests from file descriptors
        for(index=0; index<=fdmax; index++){
            if(FD_ISSET(index, &rdset)){
                int fdClient;
                if(index == fdServerSocket){
                    //new connection request, accept connection
                    SYSCALL((fdClient = accept(fdServerSocket, (struct sockaddr*)NULL, NULL)), "accept");

                    fprintf(stdout, "SERVER %d CONNECT FROM CLIENT\n", servernum);

                    FD_SET(fdClient, &set);
                    //update max fd if necessary
                    if(fdClient > fdmax) fdmax = fdClient;
                }
                else{
                    //new read request
                    fdClient = index;
                    if((estimate(fdClient)) != 0){
                        //exit(0);
                        close(fdClient);
                        FD_CLR(fdClient, &set);

                        //update fdmax
                        if(fdClient == fdmax) 
                            fdmax = updatemax(set, fdmax);
                    }
                }


            }
        }

    }

    SYSCALL(close(fdServerSocket), "close");
    exit(0);
}


/*return 1 if client fdClient closed connection, 0 if everything worked fine, -1 if some error occured while reading from the socket*/
int estimate(int fdClient){
    msg_t message;
    int index;          //index of client in connectedClients

    if(readn(fdClient, &message.len, sizeof(int)) < 0){     // nel nostro caso viene mandato 8 oppure 0
        perror("read");
        return -1;
    }

    printf("len: %d\n", message.len);

    if(readn(fdClient, &message.id, ID_SIZE) < 0){
        perror("read");
        return -1;
    }

    //conversion from network byte order to host byte order
    uint64_t newClientID = NTOHLL(message.id);

    if(message.len <= 0){
        //client closed connection
        
        if((index = member(newClientID, connectedClients)) >= 0){
            fprintf(stdout, "SERVER %d CLOSING %lx ESTIMATE %d\n", servernum, connectedClients[index].client_id, connectedClients[index].estSecret);

            //create new info structure
            info newinfo;
            newinfo.client_id = connectedClients[index].client_id;
            newinfo.estimatedSecret = connectedClients[index].estSecret;
            newinfo.nServer = 1;
            newinfo.next = NULL;
            //send estimated secret to supervisor (if estimate > 0)
            if((write(pipess, &newinfo, sizeof(newinfo))) < 0){
                perror("write");
                exit(EXIT_FAILURE);
            }

            //free(newinfo);
            //delete client from connectedClients
            est_t tmp;
            tmp = connectedClients[index];
            connectedClients[index] = connectedClients[nClients-1];
            connectedClients[nClients-1] = tmp;
            nClients--;
        }
        return 1;   //client closed connection
    }

    //nuova stima ricevuta
    long ms;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    ms = (long)(spec.tv_sec * 1000) + (long)(spec.tv_nsec / 1.0e6);

    if((index = member(newClientID, connectedClients)) < 0){         //new client
        nClients = nClients + 1;

        //first client
        if(nClients == 1){
            CHECKNULL((connectedClients=malloc(sizeof(est_t))), "malloc");
        }
        //reallocation of memory
        else CHECKNULL((realloc((est_t*)connectedClients, nClients*sizeof(est_t))), "realloc");

        //CALCOLO DEL TEMPO CORRENTE IN MILLISECONDI

        //add new client at the end of the array
        connectedClients[nClients-1].client_id = newClientID;
        connectedClients[nClients-1].t = ms;
        connectedClients[nClients-1].estSecret = -1;
        fprintf(stdout, "SERVER %d INCOMING FROM %lx @ %ld\n", servernum, connectedClients[nClients-1].client_id, connectedClients[nClients-1].t);
    }
    else{       //already in the list, index >= 0

        fprintf(stdout, "SERVER %d INCOMING FROM %lx @ %ld\n", servernum, connectedClients[index].client_id, ms);

        int tmp_est = (int)(ms - connectedClients[index].t);
        connectedClients[index].t = ms;
        if(connectedClients[index].estSecret < 0)
            connectedClients[index].estSecret = tmp_est;
        else{
            if(tmp_est < connectedClients[index].estSecret)
                //update estimate if the new one is better
                connectedClients[index].estSecret = tmp_est;
        }

    }

    //value read and updated (if needed)
    return 0;
}


// update max file descriptor
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    return -1;
}


int member(uint64_t id, est_t* list){
    for(int i = 0; i < nClients; i++){
        if(list[i].client_id == id) 
            return i;
    }
    return -1;
}


void cleanup(){
    unlink(socketname);
    if(nClients > 0){
        free(connectedClients);
    }

}