#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>

#ifdef LINUX
union semun
{
  int val;			/* value for SETVAL */
  struct semid_ds *buf;		/* buffer for IPC_STAT, IPC_SET */
  unsigned short *array;	/* array for GETALL, SETALL */
  /* Linux specific part: */
  struct seminfo *__buf;	/* buffer for IPC_INFO */
};
#endif

int
semcall (int sid, int op)
{
//    printf("\nHere\n");
  int rc = 0;
  struct sembuf sb;

  sb.sem_num = 0;
  sb.sem_op = op;
  /*
   *      * sb.sem_flg = IPC_NOWAIT;
   *           */
  sb.sem_flg = 0;
  rc = semop (sid, &sb, 1);

  return rc;
}

int getsemvalue(int sid) {
    int res=0;

    res = semctl(sid, 0, GETVAL);

    return( res );
}

int setsemvalue(int sid,int arg) {
    int res=0;

    res =  semctl(sid, 0, SETVAL, arg);

    return( res );
}


