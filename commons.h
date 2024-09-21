#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "structs_definitions.h"

#define TEST_ERROR    if (errno) {dprintf(STDERR_FILENO, "%s:%d: PID=%5d: Error %d (%s)\n", \
					  	__FILE__,	__LINE__, getpid(), errno, strerror(errno));}


/*-------------MSG-------------*/
typedef struct {
    long mtype;
    double mtext;
} my_msgbuf;

#define MSG_KEY 0x123456
#define MSG_DOCKS 01010101
#define MSG_MERCH 987650
#define MSGTYPE_RM 1
/*-------------MSG-------------*/


/*-------------SEM-------------*/
#define SEM_KEY 0x0a1
#define SEM_MASTER 0x0abc

union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO
				    (Linux-specific) */
};
/*-------------SEM-------------*/


/*-------------SHM-------------*/
#define SHM_KEY_MASTER 0x012345
#define SHM_KEY_MASTER_LOT 0x0654321
#define SHM_KEY_IN 111111
#define SHM_KEY_OUT 222222
#define SHM_KEY_INFO 333333
#define SHM_DUMP_P 444444
#define SHM_DUMP_S 555555
#define SHM_DUMP_D 666666
#define SHM_KEY_HOLD 777777
#define SHM_KEY_PARAMS 888888
/*-------------SHM-------------*/
