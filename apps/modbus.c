#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <sqlite3.h>
#include <errno.h>
#include "test.h"

#define MAX_RTU 32
#define RETRIES 3  // If a unit times out try n times.

// char results[MAX_RESULTS];

void usage() {
    // "b:d:hl:?p:r:s:t:vF:T:")
    printf("\nUsage: modbus\n");
    printf("\t-b <rate>\tBaud rate.\n");
    printf("\t-d <secs>\tDelay between retries, in seconds.\n");
    printf("\t-f <func>\tModbus function code.\n");
    printf("\t-l <7|8>\tWord length.\n");
    printf("\t-p <E|O|N>\tParity.\n");
    printf("\t-r <n>\t\tNumber of retries, before moving to next RTU.\n");
    printf("\t-v\t\tDisply verbose messages.\n");
    
    printf("\n\tDefault settings are:\n");
    printf("\t-b 9600 -d 5 -l 8 -p E -r 3 -s 2 -t /dev/ttyUSB0 -F 1 -T 32\n");
    printf("\n");
}
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
            
            printf(":%04d", data[j]);
        }
        printf("\n");
    }
    printf("==+=========================================\n");

}




int main(int argc, char *argv[]) {
    int nb_points=10;
    uint16_t *tab_rp_registers;
    
    int rc = -1;
    int rtu=0x01;
    int rtuArray[MAX_RTU];
    int i=0;
    
    char ch;
    char *tmp;
    int verbose=0;
    
    char *tty;
    char parity='E';
    int baud_rate=9600;
    int stop_bits=2;
    int length=8;
    
    modbus_t *ctx;
    
    int nap_time=5;
    int retries=3;
    int retry_count=0;
    int func=0;
    int address=0;
    char *end;
    int modbus_base_address=0;  // The offset into the modbus registers.
    int count=1;
    int exitFlag=0;
    
#ifdef LINUX
    tty=strsave( "/dev/ttyUSB0" );
#endif
    
#ifdef CYGWIN
    tty=strsave( "/dev/ttyS0" );
#endif
    
#ifdef MACOS
    tty=strsave( "/dev/ttyUSB0" );
#endif
    
    while ((ch = getopt( argc, argv, "a:b:c:d:f:hl:?p:r:s:t:v")) != -1 ) {
        switch (ch) {
            case 'a':   // baud rate
                address=strtod( optarg, &end);
                break;
            case 'b':   // baud rate
                baud_rate=atoi(optarg);
                break;
            case 'c':
                count= strtod( optarg, &end);
                break;
            case 'd':   // delay between retries 
                nap_time=atoi( optarg );
                break;
            case 'f':   // ModBus Function code
                func=atoi( optarg );
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'l':   // word length (should be 8 or 7)
                length=atoi( optarg );
                break;
            case 'p':   // parity TODO Check for valid values
                parity=*optarg;
                break;
            case 'r':   // retries
                retries=atoi( optarg ); // limit to 5
                break;
            case 's':   // stop bits (1 or 2)
                stop_bits=atoi( optarg );
                break;
            case 't':   // tty
                if(!tty) {
                    free(tty);
                }
                tty=strsave( optarg );
            case 'v':
                verbose=-1;
                break;
            default:
                break;
        }
    }
    
    if(!tty) {
        tty=(char *)strsave("/dev/ttyUSB0");
    }
    
    if( func == 0) {
        fprintf(stderr,"\nERROR: You need to set a function code.\n\n");
        usage();
        exit(-1);
    }
    
    if(verbose != 0) {
        printf("==========================\n");
        
        printf("\n\tSettings\n\n");
        printf("Serial Port:%s\n", tty);
        printf("Baud Rate  :%d\n",baud_rate);
        printf("Parity     :%c\n",parity);
        printf("Word length:%d\n", length);
        printf("Stop bits  :%d\n\n", stop_bits);
        printf("Retries    :%3d\n", retries);
        printf("Retry delay:%3d Seconds\n\n", nap_time);
        
        printf("Function   :%02d:%02x:\n", func, func );
        printf("Address    :%04d:%04x:\n",address, address);
        printf("==========================\n");
        printf("\n");
    }
    
    
    (void *)memset( &rtuArray[0],0,sizeof(rtuArray));
    
    //    ctx = modbus_new_rtu(tty, 9600, 'E', 8, 2);
    ctx = modbus_new_rtu(tty, baud_rate, parity, length, stop_bits);
    
    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }
    
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n",modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }    
    
    tab_rp_registers = (uint16_t *)malloc( nb_points * sizeof(uint16_t));
    
    if( !tab_rp_registers ) {
        fprintf(stderr,"Failed to allocate memory at %d\n", __LINE__);
    }
    
    /*
     * Set modbus RTU id.
     */
    modbus_set_slave(ctx, rtu);
    
    retry_count=retries;
    
    while (( retry_count > 0) && (exitFlag == 0) ) {
        printf("RTU=%3d\tretries=%2d\r",rtu,retry_count);
        fflush(stdout);
        
        switch( func ) {
            case 1: // read coil status
                rc = modbus_read_bits(ctx, address, count, tab_rp_registers);
                if (rc < 0) {
                    printf("%s\n",modbus_strerror(errno));
                } else {
                    printf("%d bytes returned\n",rc);
                    dump_packet( tab_rp_registers, count);
                    exitFlag=1;
                }

                break;
            case 3: // read holding registers
                
                rc = modbus_read_input_registers(ctx, address, count, tab_rp_registers);
//              printf("modbus_read_registers: %d\n",rc);                
                if (rc < 0) {
                    printf("%s\n",modbus_strerror(errno));
                } else {
                    printf("%d bytes returned\n",rc);
                    dump_packet( tab_rp_registers, count);
                    exitFlag=1;
                }
                break;
            
            default:
                printf("ERROR\n");
                exit(-1);

        }
        
        
        if (rc > 0) {
            retry_count = 0;
            
            //  printf("\rRTU %3d Found            ", rtu);
            //                dump_packet(tab_rp_registers, nb_points);
            //                exit(0);
            
            //                printf("=============================\n");
        } else {
            retry_count--;
//            if( retry_count == 0) {
//                printf("\rRTU %3d FAILED          ", rtu);
//            }
            
            sleep(nap_time);
        }
        if( (retry_count == 0) && (exitFlag == 0) ) {
            printf("\nRTU %3d FAILED          ", rtu);
        }
    }
    
    modbus_close( ctx );
    
    return(0);
    
}


