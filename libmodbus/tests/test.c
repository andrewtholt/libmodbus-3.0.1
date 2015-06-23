#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <sqlite3.h>

#include "test.h"

#define MAX_RESULTS 10240 // This is the maximum anticipated length, from a query
#define MAX_RTU 32
#define RETRIES 3  // If a unit times out try n times.
/*
 Offsets into ModBus data.
 */
#define L1_CURRENT 0
#define L2_CURRENT 1
#define L3_CURRENT 2

#define L1_VOLTS 4
#define L2_VOLTS 5
#define L3_VOLTS 6

#define L1_POWER 88
#define L2_POWER 89
#define L3_POWER 90


#define AVG_POWER 91

char results[MAX_RESULTS];

int max(int a,int b) {
    int c;

    c = ( ((a) > (b)) ? (a) : (b) );
    return(c);
}

char *strsave(char *s) {
    char           *p;

    if ((p = (char *) malloc(strlen(s) + 1)) != NULL)
        strcpy(p, s);
    return (p);
}

void dump_packet( uint16_t *data, int count) {
    int i=0;
    int j=0;

    printf("Register count: %d\n",count);

    printf("  |   00   01   02   03   04   05   06   07 \n");
    printf("==+=========================================\n");
    for(i=0; i < count; i=i+0x08) {
        printf("%02d|",i);
        for(j=i; ((j< i+0x08) && (j <= count)); j++) {
//            printf("\t%02x:\n",j);

            printf(":%04x", data[j]);
        }
        printf("\n");
    }

}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for(i=0; i<argc; i++){
        if(i == 0 ) {
            strcat(results, (argv[i] ? argv[i] : "NULL"));
        } else {
            strcat(results,":");
            strcat(results, (argv[i] ? argv[i] : "NULL"));
        }
    }
    strcat(results,";");
    return 0;
}


int main() {
    int nb_points=50;
    uint16_t *tab_rp_registers;
    int rc = -1;
    int rtu=0x01;
    int rtuArray[MAX_RTU];
    int i=0;

    sqlite3 *db;
    char *zErrMsg = 0;
    char sql[1024];
    char *tmp;

    char *tty;
    char parity;
    int baud_rate;
    int stop_bits;
    int length;
    int nap_time=30;  // Fasted sample rate allowed is 2 times a minute.
    int retries=RETRIES;
    
    int modbus_base_address=0;  // The offset into the modbus registers.

    tmp=getenv("DATABASE");

    if( !tmp ) {
        tmp=strsave("/var/data/RS.db");
    }

    printf("Opening db %s\n",tmp);
    rc=sqlite3_open(tmp,&db);

    if ( rc != SQLITE_OK) { // something went wrong
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close( db );
        exit(-1);
    }
    /*
     * The config table should have a single row.  I have not assumed that this
     * constraint is enforced by the database, so the follwoing SQL statement,
     * select the wows by descinding index, i.e. the newest first,
     * and then limits the number of rows to 1.
     *
     * A potetntial benefit is that I could keep old configs and then revert by
     * deleteing the newset.
     * 
     */
    strcpy(sql,"select tty_port,baud_rate, parity,stop_bits,length,freq from config order by idx desc limit 1;");

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    printf("res=%s\n",results);

    tty=strtok(results,":");

    if(!tty) {
        fprintf(stderr,"tty NULL\n");
        exit(1);
    }

    tmp=strtok(NULL,":");

    if(!tmp) {
        fprintf(stderr,"baud NULL\n");
        exit(1);
    }
    baud_rate=atoi(tmp);

    tmp=strtok(NULL,":");

    if(!tmp) {
        fprintf(stderr,"parity NULL\n");
        exit(1);
    }
    parity=*tmp;

    tmp=strtok(NULL,":");

    if(!tmp) {
        fprintf(stderr,"stop bits NULL\n");
        exit(1);
    }
    stop_bits=atoi(tmp);

    tmp=strtok(NULL,":");

    if(!tmp) {
        fprintf(stderr,"length  NULL\n");
        exit(1);
    }
    length=atoi(tmp);

    tmp=strtok(NULL,":");

    if(!tmp) {
        fprintf(stderr,"freq  NULL\n");
        exit(1);
    }
    printf("tmp=%s\n",tmp);
    nap_time=max(nap_time,atoi(tmp));

    printf("nap_time=%d\n",nap_time);


//    tty=strsave("/dev/tty.usbserial-A600drA9");
    modbus_t *ctx;

//    ctx = modbus_new_rtu(tty, 9600, 'E', 8, 2);
    ctx = modbus_new_rtu(tty, baud_rate, parity, length, stop_bits);

    (void) memset(results,0x00,sizeof(results));

    strcpy(sql,"select distinct(rtu) from data ;");
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    /*
    printf("=================================\n");
    printf("res=%s\n",results);
    printf("=================================\n");
     */

    (void *)memset( &rtuArray[0],0,sizeof(rtuArray));
    tmp=strtok(results,";");

    while(tmp != 0) {
        i=atoi(tmp);
        printf("%d\n",i);

        rtuArray[i]=-1;
        tmp=strtok(NULL,";");
    }



    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }
    while (TRUE) {
        strcpy(sql,"select freq from config order by idx desc limit 1;");
        memset( results,0,sizeof(results));
        
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
       
        nap_time = max(atoi(results), 30);
        printf(">>%d\n",atoi(results) );
        for(i=1;i<MAX_RTU;i++) {
            
            
            if( rtuArray[i] == -1 ) {
                rtu=i;
                //    modbus_set_debug(ctx, TRUE);
                
                if (modbus_connect(ctx) == -1) {
                    fprintf(stderr, "Connection failed: %s\n",modbus_strerror(errno));
                    modbus_free(ctx);
                    return -1;
                }
                
                //    nb_points = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ?  UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
                tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
                memset(tab_rp_registers, 0, nb_points * sizeof(uint16_t));
                
                /*
                 * Set modbus RTU id.
                 */
                modbus_set_slave(ctx, rtu);
                
                retries=RETRIES;
                
                while (retries > 0) {
                    printf("RTU=%d\nretries=%d\n",rtu,retries);
                    
                    //    rc = modbus_read_registers(ctx, 00, nb_points, tab_rp_registers);
                    
                    modbus_base_address=88;
                    
                    rc = modbus_read_input_registers(ctx, 00, nb_points, tab_rp_registers);
                    printf("modbus_read_registers: %d\n",rc);
                    if (rc > 0) {
                        retries = 0;
                        dump_packet(tab_rp_registers, nb_points);
                        
                        printf("\n");
                        printf("L1 %4d Watts\n", tab_rp_registers[L1_POWER - modbus_base_address] );
                        printf("L2 %4d Watts\n", tab_rp_registers[L2_POWER - modbus_base_address] );
                        printf("L3 %4d Watts\n", tab_rp_registers[L3_POWER - modbus_base_address] );
                        printf("=============================\n");
                        /*
                        printf("L1 %4d Volts\n", tab_rp_registers[L1_VOLTS] );
                        printf("L2 %4d Volts\n", tab_rp_registers[L2_VOLTS] );
                        printf("L3 %4d Volts\n", tab_rp_registers[L3_VOLTS] );
                        printf("=============================\n");
                        */
                        sprintf(sql,"insert into data (RTU,phaseA,phaseB,phaseC) values( %d,%d,%d,%d);", rtu,tab_rp_registers[0],tab_rp_registers[1],tab_rp_registers[3]);
                        printf("%s\n", sql);
                        
                        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
                        if( rc!=SQLITE_OK ) {
                            fprintf(stderr, "SQL error: %s\n", zErrMsg);
                            sqlite3_free(zErrMsg);
                        }
                        /*
                        modbus_base_address=L1_POWER;
                        rc = modbus_read_input_registers(ctx, modbus_base_address, nb_points, tab_rp_registers);
                        if ( rc > 0) {
                            printf("---->>>>>>> modbus_read_registers: %d\n",rc);
                            dump_packet(tab_rp_registers, nb_points);
                            
                            printf("\n");
                            printf("L1 %4d Watts\n", tab_rp_registers[L1_POWER - modbus_base_address] );
                            printf("L2 %4d Watts\n", tab_rp_registers[L2_POWER - modbus_base_address] );
                            printf("L3 %4d Watts\n", tab_rp_registers[L3_POWER - modbus_base_address] );
                            
                            printf("=============================\n");
                            
                            sprintf(sql,"insert into data (RTU,phaseAKWH,phaseBKWH,phaseCKWH) values( %d,%d,%d,%d);",
                                    rtu,
                                    tab_rp_registers[0],
                                    tab_rp_registers[1],
                                    tab_rp_registers[2]
                                    );
                            printf("%s\n", sql);
                            
                            rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
                            if( rc!=SQLITE_OK ) {
                                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                                sqlite3_free(zErrMsg);
                            }
                        }
                         */
                    } else {
                        printf("FAILED RTU %d)\n", rtu);
                        retries--;
                        sleep(1);
                    }
                }

                modbus_close( ctx );
            }
        }
        printf("Sleeping for %d\n",nap_time);
        sleep( nap_time);
    }
    sqlite3_close(db);
    return(0);
    
}


