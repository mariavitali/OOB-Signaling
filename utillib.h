#if !defined(UTILLIB_H_)
#define UTILLIB_H_

/*libreria usata come appoggio per funzioni ausiliarie, funzioni di stima, stampe,
 controlli errori syscall, ecc
*/

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>



#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

/*error checking*/
#define SYSCALL(value, message) \
    if((value) < 0){ \
        perror(message); \
        exit(errno); \
    }

#define SYSCALL_NOEXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
    }

#define CHECK_EQ(name, X, val, str, ...)	\
    if ((X)==val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define CHECK_NEQ(name, X, val, str, ...)	\
    if ((X)!=val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define CHECKNULL(value, message) \
    if ((value)==NULL){ \
        perror(message); \
        exit(EXIT_FAILURE); \
    }


#define RANDOM(X) rand() % (X)
#define RANDOMIZE(PID) srand((unsigned)time(NULL) * (PID))




//data structure to save pipe file descriptors
typedef struct pipefd{
    int fd[2];
}pipefd;


//data structure for server estimates
typedef struct estimate{
    uint64_t client_id;
    long t;      //number of milliseconds seconds since the epoch
    int estSecret;
}est_t;

//data structure for supervisor estimates (computed after receiving info from different servers)
typedef struct info{
    uint64_t client_id;
    int estimatedSecret;
    int nServer;
    struct info* next;
}info;

typedef info* infotable;





//function to print info table
void printInfotable(infotable t, FILE* where){
    infotable curr;
    curr = t;
    while(curr != NULL){
        fprintf(where, "SUPERVISOR ESTIMATE %d FOR %lx BASED ON %d\n", curr->estimatedSecret, curr->client_id, curr->nServer);
        curr = curr->next;
    }
}


#endif