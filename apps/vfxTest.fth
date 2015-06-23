
\ Library: libc.so.6

Extern: int shmget(
    int32 key,
    int32 size,
    int32 shmflag
);


Extern: void * shmat(
    int32 shmid,
    void * shmaddr,
    int32 shmflag
);

Extern: int shmdt(
    void * ptr
);

#define IPC_CREAT 0x200 \ Octal 01000
#define OWNER_RW  0x180 \ Octal 0600
#define GETVAL 5        \ Return value of semval (read)
#define SETVAL 8

Extern: int semget(
    int32 key,
    int nsems,
    int semflg
);

Extern: int semctl
    int semid,
    int semnum,
    int cmd,
);

\ Library: libhelper.so
[defined] Target_386_OSX [if]
Library: libhelper.dylib
[then]

[defined] Target_386_Linux [if]
Library: libhelper.so
[then]

Extern: int semcall(
    int sid,
    int op
);

Extern: int getsemvalue(
    int sid
);

Extern: int setsemvalue(
    int sid,
    int arg
);

-1 value shmid
-1 value ptr
-1 value semid

: init
    910 512 896 shmget to shmid
    shmid 0 896 shmat to ptr
    911 1 OWNER_RW IPC_CREAT or semget to semid
; 

: lock ( semid -- )
    -1 semcall drop
;

: release ( semid -- )
    1 semcall drop
;

: sem@ ( semid -- val )
    getsemvalue 
;


: sem! ( val semid -- )
    swap setsemvalue 
;


init

semid sem@

