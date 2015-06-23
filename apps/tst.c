#include <stdio.h>
#include <sys/sem.h>

#define SEM_KEY 901


int main() {
    int semid=-1;
    int rc=0;

    semid = semget(SEM_KEY,1,0666 | IPC_CREAT);

    printf("semget\t:%d\n",semid);

    rc=getsemvalue( semid );
    printf("sem value\t:%d\n",rc);

    printf("Set sem value\n");
    rc=setsemvalue( semid, 2 );

    rc=getsemvalue( semid );
    printf("sem value\t:%d\n",rc);

    printf("Aquire lock\n");
    rc=semcall(semid,-1);
    printf("\trc=%d\n",rc);

    rc=getsemvalue( semid );
    printf("sem value\t:%d\n",rc);
}

