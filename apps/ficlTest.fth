load mydump.fth

-1 value input_shmid
-1 value input_semid
-1 value input_ptr

-1 value holding_shmid
-1 value holding_semid
-1 value holding_ptr

900 512 896 shmget to input_shmid
.( input_shmid is ) input_shmid . cr
input_shmid shmat to input_ptr

901 semtran to input_semid
.( holding_semid is ) holding_semid . cr

910 512 896 shmget to holding_shmid
.( holding_shmid is ) holding_shmid . cr
holding_shmid shmat to holding_ptr

911 semtran to holding_semid
.( holding_semid is ) holding_semid . cr

hex

: dump-input
    cr ." Input Registers" cr cr
    input_ptr . cr

    input_semid getsem abort" getsem"
    input_ptr 20 mdump cr
    input_semid relsem abort" relsem"
;

: dump-holding
    cr ." Holding Registers" cr cr
    holding_ptr . cr

    holding_semid getsem abort" getsem"
    holding_ptr 20 mdump cr
    holding_semid relsem abort" relsem"
;

: dump-sem
    cr ." Input sema value   = " input_semid getsemvalue .
    cr ." Holding sema value = " holding_semid getsemvalue .
    cr
;

: write-holding ( 16bits word-offset -- )
    holding_semid getsem abort" getsem"
    2* holding_ptr + w!
    holding_semid relsem abort" relsem"
;

: read-holding ( word-offset -- 16bits )
    holding_semid getsem abort" getsem"
    2* holding_ptr + w@
    holding_semid relsem abort" relsem"

;

: write-input ( 16bits word-offset -- )
    input_semid getsem abort" getsem"
    2* input_ptr + w!
    input_semid relsem abort" relsem"
;


s" shm-ok> " 0 set-prompt


