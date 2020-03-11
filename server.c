#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "utillib.h"
#include "utilconn.h"


#define UNIX_PATH_MAX 108

int servernum;
int pipess;              //pipe server-supervisor
char* socketname;
est_t* connectedClients; //data structure to keep track of clients' secret estimates
static int nClients;

static volatile sig_atomic_t terminate=0;

static void sighandler(int useless) {    
    terminate=1;
}


// update max file descriptor
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    return -1;
}

void cleanup(){
    unlink(socketname);
    if(nClients > 0){
        free(connectedClients);
    }

}


void executeServer();
int estimate(int fdClient);



int main(int argc, char* argv[]){

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

    //set SIGPIPE ignore
    struct sigaction p;
    memset(&p, 0, sizeof(p));
    p.sa_handler = SIG_IGN;
    SYSCALL((sigaction(SIGPIPE, &p, NULL)), "sigaction ignore pipe");

    //id number of server
    errno = 0;
    servernum = (strtol(argv[1], NULL, 10)) + 1;        //between 1 and k
    //servernum = (int)(*argv[1]) + 1;
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    //fd pipe server-supervisor
    pipess = strtol(argv[2], NULL, 10);
    //int pipess = (int)(*argv[2]);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }


    socketname = malloc(32 * sizeof(char));
    sprintf(socketname, "OOB-server-%d", servernum);

    executeServer();

    //atexit pulizia
    atexit(cleanup);


}



void executeServer(){

    //allocate space for 1st client
    connectedClients = malloc(sizeof(est_t));


/**************************************************SOCKET CONNECTI0N SET UP***************************************************************/

    int fdServerSocket;     // listening socket

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
    int nread;      //read characters
    nClients = 0;   //number of clients currently connected

    if(fdServerSocket > fdmax)
        fdmax = fdServerSocket;
    
    FD_ZERO(&set);      //set to zero
    FD_ZERO(&rdset);
    FD_SET(fdServerSocket, &set);

    while(!terminate){
        //get select mask ready (at every new iteration, cause is modified by select)
        rdset = set;
        SYSCALL(select(fdmax+1, &rdset, NULL, NULL, NULL), "select");

        //check for new requests from file descriptors
        for(int i=0; i<fdmax; i++){
            if(FD_ISSET(i, &rdset)){
                int fdClient;
                if(i == fdServerSocket){
                    //new connection request, accept connection
                    SYSCALL((fdClient = accept(fdServerSocket, (struct sockaddr*)NULL, NULL)), "accept");

                    fprintf(stdout, "SERVER %d CONNECT FROM CLIENT\n", servernum);

                    FD_SET(fdClient, &set);
                    //update max fd if necessary
                    if(fdClient > fdmax) fdmax = fdClient;
                }
                else{
                    //new read request
                    fdClient = i;
                    if((estimate(fdClient)) != 0){
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

    //LIBERARE LA MEMORIAAAA
    SYSCALL(close(fdServerSocket), "close");
    return 0;

}


/*return 1 if client fdClient closed connection, 0 if everything worked fine, -1 if some error occured while reading from the socket*/
int estimate(int fdClient){
    msg_t message;
    int index;          //index of client in connectedClients

    if(readn(fdClient, (int)&message.len, sizeof(int)) < 0){     // nel nostro caso viene mandato 8 oppure 0
        perror("read");
        return -1;
    }

    if(readn(fdClient, (uint64_t)&message.id, ID_SIZE) < 0){
        perror("read");
        return -1;
    }

    //conversion from network byte order to host byte order
    uint64_t newClientID = NTOHLL(message.id);

    if(message.len == 0){
        //client closed connection
        
        if((index = member(newClientID, connectedClients)) >= 0){
            fprintf(stdout, "SERVER %d CLOSING %ld ESTIMATE %d\n", servernum, connectedClients[index].client_id, connectedClients[index].estSecret);

            //create new info structure
            info* newinfo = malloc(sizeof(info));
            newinfo->client_id = connectedClients[index].client_id;
            newinfo->estimatedSecret = connectedClients[index].estSecret;
            newinfo->nServer = 1;
            newinfo->next = NULL;
            //send estimated secret to supervisor (if estimate > 0)
            if((write(pipess, newinfo, sizeof(newinfo))) < 0){
                perror("write");
                exit(EXIT_FAILURE);
            }

            free(newinfo);
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
    if((index = member(newClientID, connectedClients)) < 0){         //new client
        nClients = nClients + 1;

        //first client
        if(nClients == 1){
            CHECKNULL((malloc(sizeof(est_t))), "malloc");
        }
        //reallocation of memory
        else CHECKNULL((realloc((est_t*)connectedClients, nClients*sizeof(est_t))), "realloc");

        //add new client at the end of the array
        connectedClients[nClients-1].client_id = newClientID;
        connectedClients[nClients-1].t = (int)time(NULL);
        connectedClients[nClients-1].estSecret = -1;
        fprintf(stdout, "SERVER\t%d\tINCOMING FROM\t%lx\t@\t%d\n", servernum, connectedClients[nClients-1].client_id, connectedClients[nClients-1].t);
    }
    else{       //already in the list, index >= 0
        int curr_time = ((int)time(NULL)) * 1000;

        fprintf(stdout, "SERVER\t%d\tINCOMING FROM\t%lx\t@\t%d\n", servernum, connectedClients[index].client_id, connectedClients[index].t);

        int tmp_est = curr_time - connectedClients[index].t;
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


int member(uint64_t id, est_t* list){
    for(int i = 0; i < nClients; i++){
        if(list[i].client_id == id) 
            return i;
    }
    return -1;
}