#if !defined(UTILLIB_H_)
#define UTILLIB_H_

/*libreria usata come appoggio per funzioni ausiliarie, funzioni di stima, stampe,
 controlli errori syscall, ecc
*/

/*FARE 2 DIVERSE LIBRERIE. UNA PER COMUNICAZIONI SUPERVISOR-SERVER
 L'ALTRA DI CONNESSIONE SOCKET PER COMUNICAZIONI CLIENT-SERVER*/

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


#define RANDOM(X) rand() % (X)                        //http://www.pierotofy.it/pages/guide_tutorials/C/Generazione_di_numeri_casuali/
#define RANDOMIZE srand((unsigned)time(NULL))



//new data structures
typedef struct pipefd{
    int fd[2];
}pipefd;


//struttura dati per memorizzare le stime del server
typedef struct estimate{
    uint64_t client_id;
    long t;      //number of milliseconds seconds since the epoch
    int estSecret;
}est_t;

//struttura dati per memorizzare le stime del supervisor dopo aver raccolto le informazioni inviate da vari server
typedef struct info{
    uint64_t client_id;
    int estimatedSecret;
    int nServer;
    struct info* next;
}info;

typedef info* infotable;

//supervisor function to print info table
void printInfotable(infotable t, FILE* where){
    infotable curr;
    curr = t;
    while(curr != NULL){
        fprintf(where, "SUPERVISOR ESTIMATE\t%d\tFOR\t%lx\tBASED ON\t%d\n", curr->estimatedSecret, curr->client_id, curr->nServer);
        curr = curr->next;
    }
}


#endif