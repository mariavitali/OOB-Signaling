#define _POSIX_C_SOURCE 199309
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>


#include "utilconn.h"
#include "utillib.h"

uint64_t rand64bit();
void connectToServers(int p, int k);
void sendMessages(int w, int p, uint64_t id);



int* selectedServers;           //p servers to connect to
static struct timespec t;

int main(int argc, char* argv[]){

    int p, k, w;
    int secret;
    uint64_t id;

    
    //check main arguments
    if(argc < 4){
        fprintf(stderr, "Use: %s p k w (int)\np = number of server to connect to\nk = number of available servers\nw = number of messages to send\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //convert to int
    errno = 0;
    p = strtol(argv[1], NULL, 10);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }
    k = strtol(argv[2], NULL, 10);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }
    w = strtol(argv[3], NULL, 10);
    if(errno != 0){
        fprintf(stderr, "strtol error\n");
        exit(errno);
    }

    //check parameters
    if(p < 1 || p > (k-1)){
        fprintf(stderr, "error: use 1 <= p < k\n");
        exit(EXIT_FAILURE);
    }
    if(w <= (3*p)){
        fprintf(stderr, "error: use w > 3p\n");
        exit(EXIT_FAILURE);
    }


    //generate secret and random 64-bit id
    RANDOMIZE;
    secret = RANDOM(3000) + 1;
    id = rand64bit();

    fprintf(stdout, "CLIENT %lx SECRET %d\n", id, secret);

    //initialize structure timespec for nanosleep based on secret
    
    t.tv_sec = (int)(secret/1000);
    t.tv_nsec = (secret % 1000) * 1000000;

    CHECKNULL((selectedServers = malloc(p*sizeof(int))), "malloc");
    connectToServers(p, k);
    sendMessages(w, p, id);

    fprintf(stdout, "CLIENT %lx DONE.\n", id);
    free(selectedServers);
    //LIBERARE MEMORIAAAAAAA (SE NECESSARIO)
    return 0;

}


uint64_t rand64bit()
{
    uint64_t random = 0;
    random ^= ((uint64_t)rand() & 0xFFFF);
    random ^= ((uint64_t)rand() & 0xFFFF) << 16;
    random ^= ((uint64_t)rand() & 0xFFFF) << 32;
    random ^= ((uint64_t)rand() & 0xFFFF) << 48;
    return random;
}

//connects via socket to p of the k available servers
void connectToServers(int p, int k){

    RANDOMIZE;
    int noduplicate[p];

    for(int i=0; i<p; i++){
        struct sockaddr_un serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));

        SYSCALL((selectedServers[i] = socket(DOMAIN, SOCK_STREAM, 0)), "creating socket");

        //select 1 server (ATTENZIONE: SE GENERO LO STESSO NUMERO 2 VOLTE CONSECUTIVE, CHE SUCCEDE?)
        int numserver, ok=0;;
        while(!ok){
            numserver = RANDOM(k) + 1;      //between 1 and k
            ok = 1;
            for(int j = 0; j < i; j++){
                if(numserver == noduplicate[j]) ok = 0;
            }
        }
        if(ok) noduplicate[i] = numserver;
        serverAddress.sun_family = DOMAIN;
        sprintf(serverAddress.sun_path, "OOB-server-%d", numserver);


        //connect to server numserver
        errno = 0;
        while(connect(selectedServers[i], (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
            if(errno == ENOENT){
                sleep(1);
            }
        }

        fprintf(stdout, "connected to server %d\n", numserver);

    }

}

void sendMessages(int w, int p, uint64_t id){
    //convert id to network byte order
    uint64_t id_nbo = HTONLL(id);
    int len, random;

    RANDOMIZE;

    for(int i=0; i<w; i++){
        //select one of the p connected servers
        random = RANDOM(p);
        len = ID_SIZE;

        fprintf(stdout, "sending to server on file descriptor %d the length %d and the id %lx\n", selectedServers[random], len, id_nbo);

        //GESTIRE CHIUSURA DEL SERVER -- SIGNAL SIGPIPE (perchè quando killo il server vengono eliminate le socket in lettura quindi scrivo su un file descriptor che non ha lettori-->sigpipe)
        writen(selectedServers[random], &len, sizeof(int));
        writen(selectedServers[random], &id_nbo, ID_SIZE);

        //wait secret milleseconds
        SYSCALL(nanosleep(&t, NULL), "nanosleep");

    }

    //close client
    for(int i = 0; i<p; i++){
        len = 0;
        fprintf(stdout, "sending to server on file descriptor %d the length %d and the id %lx\n", selectedServers[i], len, id_nbo);

        writen(selectedServers[i], &len, sizeof(int));
        writen(selectedServers[i], &id_nbo, ID_SIZE);

        SYSCALL((close(selectedServers[i])), "close selectedServer");
    }
}