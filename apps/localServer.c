/*
 Name: localServer.c
 Description:
 Acts as a ModBus/TCP server responding to register wide commands for
 input & holding registers only.
 
 TODO:
 
 Check permisions of shared areas.
 
 Enforce semaphores.
 
 Add function 0x05 (write single coil) to for serial attached modbus devices.
 Implement Function Code 43/24 (0x2b/0x0e) Read Device Identification, for local RTU.
 
 Add signal handler for SIGHUP.  This sets the exit flag.
 
 DONE:
 
 Connect the input register (RW to the world) to a shared memory regionn.
 Connect the holding registers (RO to the world) to a shared memory regionn.
 
 Define a semaphore for each of the above.
 
 Uses an ini file library.  
 
 Looks in /etc, /usr/local/etc, $HOME/etc, $HOME in that order.
 
 If no file exists at start up create one, in the current directory
 having the default settings.
 
 Use a common file for both this and simpleServer ignoring unapplicable settings.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <iniparser.h>

/*
 Globals.
 
 These are things that are set and then left alone.
 
 It seems pointless, to me, to keep passing these around.  Ideally there would be some way of 
 changing them to a constant at run time, or set once and that's it.
 */
int runFlag = -1;
int exitFlag = 0; // Set to non-zero to exit.

int             header_length;
modbus_t       *ctx_tcp = (modbus_t *)NULL;
int verbose=-1;
dictionary *ini;

/*
 The number of bytes to allocate for each of the areas.
 */
#define MODBUS_MAX_REGISTERS 0x2000 // 0x1000 16 bit registers
#define MODBUS_MAX_BITS 0x100       // 0x1000 single bits.

#define LOCAL_INPUT_REGISTERS 1024 // 512 RO registers
#define LOCAL_HOLDING_REGISTERS 1024 // 512 RW registers
#define RAW_REQ_LENGTH 10 

const uint16_t UT_REGISTERS_NB_SPECIAL = 0x2;

void sig_handler(int signum) {
//    exitFlag = -1;
    
    printf("Signal recieved:%02d\n",signum);
    
    if( SIGUSR1 == signum) {
        exitFlag=-1;
    }
}

char *strsave(char *s) {
    char *p;
    
    if ((p = (char *) malloc(strlen(s) + 1)) != NULL) {
        strcpy(p, s);
    }
    return (p);
}

void memDump(char *ptr, int len) {
    
    int i=0;
    int j=0;
    uint8_t data;
    
    for (i=0; i < len; i += 0x08) {
        printf("%08x",(unsigned int)(ptr+i));
        for (j=0; j<0x08; j++) {
            printf(":%02x",(uint8_t) *(ptr+i+j) );
        }
        printf(":");
        for (j=0; j<0x08; j++) {
            data= (uint8_t) *(ptr+i+j);
            
            if ((data < 0x20) || (data > 0x7f)) {
                printf(".");
            } else {
                printf("%c",data);
            }

        }
        printf("\n");
    }
    printf("\n");
}
/*
 Increment the semaphore count by op.  i.e. to get the sem, add -1 to it
 to release ad 1 to it.
 */
int semcall(int sid, int op) {
	struct sembuf   sb;
	
	sb.sem_num = 0;
	sb.sem_op = op;
	/*
	 * sb.sem_flg = IPC_NOWAIT;
	 */
	sb.sem_flg = 0;
    
	if (semop(sid, &sb, 1) == -1) 	{
		perror("ficl: semcall ");
		return (0);
	} else {
		return (-1);
	}
}

int getSem(int sid) {
    return( semcall( sid,-1));
}

int relSem(int sid) {
    return(semcall( sid,1));
}


int createIni(char *name ) {
    FILE *fd;
    
    fd=fopen( name, "w+");
    
    if (0 == fd) {
        fprintf(stderr,"Failed to create ini file\n");
        exit(-1);
    }
    
    fprintf(fd,"#\n");
    fprintf(fd,"# Default ini file.\n");
    fprintf(fd,"#\n");
    fprintf(fd,"[network]\n");
    fprintf(fd,"\tIP = 127.0.0.1 ; Loopback\n");
    fprintf(fd,"\tport = 1502 ; Unprivilidged port.  502 is the norm.\n");
    fprintf(fd,"\n");
    
    fprintf(fd,"[system]\n");
    fprintf(fd,"\tverbose = yes ; Debug messages\n");
    fprintf(fd,"\n");
    
    fprintf(fd,"\tshare_input = yes ; Create shared memory segment for input registers\n");
    fprintf(fd,"\tshare_holding = yes ; As above for holding registers\n");
    fprintf(fd,"\n");
    fprintf(fd,"\tinput_reg_size = 1024 ; 512 16 bit registers\n");
    fprintf(fd,"\tinput_reg_key = 900 ; see shmget(2)\n");
    fprintf(fd,"\tinput_reg_sema = 901 ; see semget(2)\n");
    fprintf(fd,"\n");
    
    fprintf(fd,"\tholding_reg_size = 1024\n");
    fprintf(fd,"\tholding_reg_key = 910 ; see shmget(2)\n");
    fprintf(fd,"\tholding_reg_sema = 911 ; see semget(2)\n");
    fprintf(fd,"\n");
    fprintf(fd,"\n");
    
    fprintf(fd,"[modbus]\n");
    fprintf(fd,"\tdebug = no ; Display ModBus packets ?\n");
    fprintf(fd,"\tlocal_rtu = 255 ; My ModBud address\n");
    fprintf(fd,"\tRTU = no ; Serial RTU gateway enabled ?\n");
    fprintf(fd,"\ttty = /dev/ttyUSB0 ; Serial port\n");
    fprintf(fd,"\tbaud_rate = 9600 ; serial speed\n");
    fprintf(fd,"\tlength = 8 ; Bits per byte\n");
    fprintf(fd,"\tparity = E ;\n");
    fprintf(fd,"\tstop_bits = 2 ;\n");
    fprintf(fd,"# \n");
    fprintf(fd,"# debug_03 = yes ; Debug 0x03 Read Holding Registers\n");
    fprintf(fd,"# debug_04 = yes ; Debug 0x04 Read Input Registers\n");
    fprintf(fd,"# debug_06 = yes ; Debug 0x06 Write Single Register\n");
    fprintf(fd,"# \n");
    
    fprintf(fd,"\n");
    fprintf(fd,"\n");
    
    fclose(fd);
    
    printf("\nReview config file %s.  Make any adjustments and restart.\n",name);
    exit(0);
    
}

void todo() {
    printf("\n\t\tTODO List\n\n");
    printf("\tError Checking on address/size\n");
    printf("\tCheck permisions of shared areas.\n");
    
//    printf("\tEnforce semaphores.\n");
    
    printf("\tAdd function 0x05 (write single coil) for serial attached modbus devices.\n");
    printf("\tImplement Function Code 43/24 (0x2b/0x0e) Read Device Identification, \n\tfor local RTU.\n");
    printf("\tAdd signal handler for SIGHUP.  This sets the exit flag.\n");
}


void usage() {
    todo();
    printf("\nUsage: simpleServer\n");
    printf("\t-b <rate>\tSet RTU baud rate.\n");
    printf("\t-c <inifile>\tRead this file for config.\n");
    printf("\t-h|-?\t\tHelp.\n");
    printf("\t-i <IP Addr>\tIP Address to listen on.\n");
    printf("\t-p <port num>\tNetwork port.\n");
    printf("\t-P E|O|N\tSerial port parity.\n");
    printf("\t-t <port>\tSerial port to use for ModBus RTUs\n");
    printf("\t-q\t\tQuiet.\n");
    printf("\t-v\t\tVerbose.\n");
    
    
    printf("\n");
}


void readHoldingRegisters(uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint8_t *tmp;
    uint16_t address;
    uint16_t qty;
    uint8_t *raw_query;
    uint8_t *raw_reply;
    uint8_t RTU;
    uint8_t fcode;
    
    int rc=0;
    int i=0;
    
    raw_query = malloc(RAW_REQ_LENGTH);
    raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    
    if(verbose) {
        printf("Read Holding Registers\n");
        printf("RTU    =%02d\n",RTU);
        printf("Address=%04d\n",address);
        printf("Qty    =%04d\n",qty);
    }
    
    RTU=query[header_length - 1];
    fcode=query[header_length];
    address = (query[header_length+1] << 8) + (query[header_length+2] & 0xff);
    qty = (query[header_length+3] << 8) + (query[header_length+4] & 0xff);
    
    tmp=(unsigned char *)&mb_mapping->tab_registers[address];
    
    memcpy(raw_query,&query[header_length-1], 6);
    modbus_set_slave( ctx,RTU);
    
    rc=modbus_send_raw_request( ctx,raw_query,6);
    if( -1 == rc ) {
        printf("modbus_send_raw_request: %s\n",modbus_strerror(errno));
    }
    
    rc=modbus_receive_confirmation(ctx, raw_reply);
    if( -1 == rc ) {
        printf("modbus_recieve_confirmation: %s\n",modbus_strerror(errno));
    }
    
    for(i=0;i< raw_reply[2];i=i+2 ) {
        tmp[i] =raw_reply[i+3];
        tmp[i+1]=raw_reply[i+4];
        
    }
    
    rc = modbus_reply(ctx_tcp, query, rc, mb_mapping);
    free( raw_reply);
    free( raw_query);
}

void writeMultipleRegisters(uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint16_t address;
    uint16_t qty;
    
    uint8_t *raw_query;
    uint8_t *raw_reply;
    uint8_t RTU;
    uint8_t fcode;
    int localDebug=0;
    
    localDebug = iniparser_getboolean(ini,"modbus:debug_10",0);
    localDebug=1;
    
    if(localDebug) {
        modbus_set_debug(ctx, TRUE);
    }
    modbus_set_debug(ctx, TRUE);
    raw_query = malloc(RAW_REQ_LENGTH);
    raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    
    RTU=query[header_length - 1];
    fcode=query[header_length];
    address = (query[header_length+1] << 8) + (query[header_length+2] & 0xff);
    qty = (query[header_length+3] << 8) + (query[header_length+4] & 0xff);
    
    if(verbose) {
        printf("Write Multiple Registers\n");
        printf("RTU    =%02d\n",RTU);
        printf("Address=%04d\n",address);
        printf("Qty    =0x%04x\n",qty);
    }

    memDump( &query[ header_length + 6], qty);
    
    
    free( raw_reply);
    free( raw_query);
}

void writeSingleCoil(uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint16_t address;
    uint8_t flag;
    uint8_t *raw_query;
    uint8_t *raw_reply;
    uint8_t RTU;
    uint8_t fcode;
    int localDebug=0;
    
    int rc=0;
    
    localDebug = iniparser_getboolean(ini,"modbus:debug_05",0);
    localDebug=1;
    
    if(localDebug) {
        modbus_set_debug(ctx, TRUE);
    }
    
    raw_query = malloc(RAW_REQ_LENGTH);
    raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    
    RTU=query[header_length - 1];
    fcode=query[header_length];
    address = (query[header_length+1] << 8) + (query[header_length+2] & 0xff);
    flag = query[header_length+3];
    
    if(verbose) {
        printf("Write Single Register\n");
        printf("RTU    =%02d\n",RTU);
        printf("Address=%04d\n",address);
        printf("Flag   =0x%02x\n\n",flag);
    }
    
    modbus_set_slave( ctx,RTU);
    
    memcpy( raw_query, &query[6],6);
    
    if(verbose) {
        memDump( raw_query, 0x10 );
    }
    
    rc=modbus_send_raw_request( ctx,raw_query,6);
    if (-1 == rc) {
        fprintf(stderr,"writeSingleRegister:0", modbus_strerror(errno));
    }
    rc=modbus_receive_confirmation(ctx, raw_reply);

//    memDump( raw_reply, 0x10 );
    
    memcpy( &query[6],raw_query,rc);
    
    memDump( query,rc);
    
    rc = modbus_reply(ctx_tcp, query, rc+4, mb_mapping);
    if (-1 == rc) {
        fprintf(stderr,"writeSingleRegister:1", modbus_strerror(errno));
    }

    free(raw_reply);
    free(raw_query);
}


void writeSingleRegister(uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint16_t address;
    uint16_t data;
    uint8_t *raw_query;
    uint8_t *raw_reply;
    uint8_t RTU;
    uint8_t fcode;
    int localDebug=0;
    
    int rc=0;
    
    localDebug = iniparser_getboolean(ini,"modbus:debug_06",0);
    localDebug=1;
    
    if(localDebug) {
        modbus_set_debug(ctx, TRUE);
    }
    
    raw_query = malloc(RAW_REQ_LENGTH);
    raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    
    RTU=query[header_length - 1];
    fcode=query[header_length];
    address = (query[header_length+1] << 8) + (query[header_length+2] & 0xff);
    data = (query[header_length+3] << 8) + (query[header_length+4] & 0xff);
    
    mb_mapping->tab_registers[address]=data;
    
    if(verbose) {
        printf("Write Single Register\n");
        printf("RTU    =%02d\n",RTU);
        printf("Address=%04d\n",address);
        printf("Data   =0x%04x\n",data);
    }
    
    modbus_set_slave( ctx,RTU);
    memcpy(raw_query,&query[header_length-1], 6);
    
    rc=modbus_send_raw_request( ctx,raw_query,6);
    if (-1 == rc) {
        fprintf(stderr,"writeSingleRegister:0", modbus_strerror(errno));
    }
    rc=modbus_receive_confirmation(ctx, raw_reply);
    
    memcpy( &query[6],raw_query,rc);
    
    memDump( raw_query,rc);
    
    rc = modbus_reply(ctx_tcp, query, rc+4, mb_mapping);
    if (-1 == rc) {
        fprintf(stderr,"writeSingleRegister:1", modbus_strerror(errno));
    }
    
    rc = modbus_flush( ctx_tcp);
    
    printf("flushed=%d\n", rc);
    
    if(localDebug) {
        modbus_set_debug(ctx_tcp, FALSE);
    }

    free(raw_reply);
    free(raw_query);
}

void readInputRegisters(uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint8_t *tmp;
    uint16_t address;
    uint16_t qty;
    uint8_t *raw_query;
    uint8_t *raw_reply;
    uint8_t RTU;
    uint8_t fcode;
    
    int rc=0;
    int i=0;
    
    raw_query = malloc(RAW_REQ_LENGTH);
    raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    
    if(verbose) {
        printf("Write Input Registers\n");
        printf("RTU    =%02d\n",RTU);
        printf("Address=%04d\n",address);
        printf("Qty    =%04d\n",qty);
    }
    
    RTU=query[header_length - 1];
    fcode=query[header_length];
    address = (query[header_length+1] << 8) + (query[header_length+2] & 0xff);
    qty = (query[header_length+3] << 8) + (query[header_length+4] & 0xff);
    
    tmp=(unsigned char *)&mb_mapping->tab_input_registers[address];
    
    memcpy(raw_query,&query[header_length-1], 6);
    modbus_set_slave( ctx,RTU);
    
    rc=modbus_send_raw_request( ctx,raw_query,6);
    if( -1 == rc ) {
        printf("modbus_send_raw_request: %s\n",modbus_strerror(errno));
    }
    
    rc=modbus_receive_confirmation(ctx, raw_reply);
    if( -1 == rc ) {
        printf("modbus_recieve_confirmation: %s\n",modbus_strerror(errno));
    }
    
    for(i=0;i< raw_reply[2];i=i+2 ) {
        tmp[i] =raw_reply[i+3];
        tmp[i+1]=raw_reply[i+4];

    }

    rc = modbus_reply(ctx_tcp, query, rc, mb_mapping);
    free( raw_reply);
    free( raw_query);
}

void handleSerialRTU( uint8_t *query, modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    uint8_t fcode; // ModBus function code
    
    fcode=query[header_length];
    
    switch (fcode) {
        case 0x03:
            readHoldingRegisters(query, ctx, mb_mapping);
            break;
        case 0x04:
            readInputRegisters(query, ctx, mb_mapping);
            break;
        case 0x05: // Write single coil.
            writeSingleCoil(query, ctx, mb_mapping);
            break;
        case 0x06:
            writeSingleRegister(query, ctx, mb_mapping);
            break;
        case 0x10:
            writeMultipleRegisters(query, ctx, mb_mapping);
            break;
        default:
            break;
    }
}


int main(int argc, char *argv[]) {

    modbus_t    *ctx_serial = (modbus_t *)NULL;
    
    
	uint8_t        *query;
    uint8_t         *raw_query;
    uint8_t        *raw_reply;
	
	int             socket;
    int rc;
    int i=0;
    
    modbus_mapping_t *local_mb_mapping;
    modbus_mapping_t *mb_mapping;
    
    int localRTU=255;
    int RTU;
    
    int cnt=1;
    
    char *ip=(char *)NULL;
    int port=0;
    
    int io_address=0;
    
    uint16_t *localInputRegisters;
    int localInputRegistersSize;
    
    uint16_t *localHoldingRegisters;
    int localHoldingRegistersSize;
    
    char ch;    // for getopt
    int opt;

    char scratch[255];
    char *ptr;
    char *iniFile=(char *)NULL;
    
    
    int useSharedInput=0;
    int useSharedHolding=0;
    
    int holdingShmKey=0;
    int holdingShmId=0;
    
    int holdingSemKey=0;
    int holdingSemId=0;
    
    
    int inputShmKey=0;
    int inputShmId=0;
    
    int inputSemKey=0;
    int inputSemId=0;
    
    int modbusDebug = 0;
    int serialRTUenabled = 0;
    char *tty=(char *)NULL;
    char *tmp;
    int baud_rate=9600;
    char parity='E';
    int length=8;
    int stop_bits=2;
    
    char *iniFileName[] = {
        "/etc/localServer.ini",
        "/usr/local/etc/localServer.ini",
        "./localServer.ini"
    };
    
    
    //    ip=strsave("127.0.0.1");    // default is localhost
    
    while ((opt = getopt(argc,argv,"c:h?i:p:qv")) != -1) {
        switch (opt) {
            case 'c': // config file
                if ((char *)NULL != iniFile) {
                    free( iniFile );
                }
                iniFile = strsave( optarg );
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
                break;
            case 'i':
                if (ip != (char *)NULL) {
                    free(ip);
                }
                ip=strsave( optarg );
                break;
            case 'p':
                port=atoi( optarg );
                break;
            case 'q':
                verbose=0;
                break;
            case 'v':
                verbose=1;
                break;
            default:
                printf("What ?\n");
                exit(-10);
                break;
        }
    }
    
    if (iniFile == (char *)NULL) {
        for(i=0;(i<3 && (iniFile == (char *)NULL))  ;i++) {
            
            if ( verbose) {
                printf("%d:%s\n",i, iniFileName[i]);
            }
            
            if( 0 == access( iniFileName[i],R_OK)) {
                if(verbose ) {
                    printf("\tFile exists\n");
                }
                iniFile = strsave( iniFileName[i]);
            }
        }
    }
    
    
    
    if (iniFile == (char *)NULL ) {
        printf("No existing ini file.\n");
        // Create an default file
        
        iniFile=strsave("./localServer.ini");
        printf("\tCreating new ini file %s\n", iniFile);
        
        createIni( iniFile );
        exit(0);
    }

    ini = iniparser_load( iniFile );
    
    ptr= iniparser_getstring(ini,"network:IP","127.0.0.1");
    
    if ((char *)NULL == ip) {
        ip = strsave(ptr);
    }
    
    if (0 == port) {
        port = iniparser_getint(ini,"network:port",1502);
    }
    
    if (-1 == verbose) {
        verbose = iniparser_getboolean(ini,"system:verbose",0);
    }
    
    if (verbose) {
        printf("\n\n\tVerbose\n\n");
    }
    
    localHoldingRegistersSize = iniparser_getint(ini,"system:holding_reg_size",
                                                 LOCAL_HOLDING_REGISTERS);
    localInputRegistersSize = iniparser_getint(ini,"system:input_reg_size",
                                               LOCAL_INPUT_REGISTERS);
    
    useSharedInput = iniparser_getboolean(ini,"system:share_input",0);
    useSharedHolding = iniparser_getboolean(ini,"system:share_holding",0);
    
    modbusDebug = iniparser_getboolean(ini,"modbus:debug",0);
    
    localRTU = iniparser_getint(ini,"modbus:LOCAL_RTU",255);
    
    serialRTUenabled = iniparser_getboolean(ini,"modbus:RTU",0);
    
    if (serialRTUenabled) {
        tty = iniparser_getstring(ini,"modbus:tty","/dev/ttyUSB0");
        baud_rate = iniparser_getint(ini,"modbus:baud_rate",9600);
        
        tmp = iniparser_getstring(ini,"modbus:parity","E");
        parity=tmp[0];
        
        length=iniparser_getint(ini,"modbus:length",8);
        stop_bits=iniparser_getint(ini,"modbus:stop_bits",2);
        
        
    }
    
    
    /*
     localInputRegisters= (uint16_t *)malloc( localInputRegistersSize );
     localHoldingRegisters = (uint16_t *)malloc( localHoldingRegistersSize);
     
     memset( localInputRegisters,0x00, localInputRegistersSize);
     memset( localHoldingRegisters,0x00, localInputRegistersSize);
     */
    
    if(verbose) {
        printf("\n\t\tSettings\n");
        printf(  "\t\t========\n\n");
        printf("\tBuild Date\t:%s\n",__DATE__);
        printf("\tAddress\t\t:%s\n",ip);
        printf("\tNetwork port\t:%d\n",port);
        printf("\n");
        printf("\tShared Memory\n");
        
        printf("\t\tShare Input\t:");
        
        if ( useSharedInput ) {
            printf("Yes\n");
        } else {
            printf("No\n");
        }
        
        printf("\t\tShare Holding\t:");
        
        if ( useSharedHolding ) {
            printf("Yes\n");
        } else {
            printf("No\n");
        }
        
        printf("\tInput Registers\t\t:%4d Bytes\n",localInputRegistersSize);
        printf("\tHolding Registers\t:%4d Bytes\n",localHoldingRegistersSize);
        
        printf("\n\tModbus\n\t\tdebug\t\t:");
        if ( modbusDebug ) {
            printf("Yes\n");
        } else {
            printf("No\n");
        }
        printf("\t\tLocal RTU\t: %03d\n",localRTU);
        printf("\t\tSerial RTU\t:");
        if ( serialRTUenabled ) {
            printf("Yes\n");
            printf("\t\t\tSerial Port\t:%s\n",tty);
            printf("\t\t\tBaud Rate\t:%d\n",baud_rate);
            printf("\t\t\tParity\t\t:%c\n", parity);
            printf("\t\t\tStop Bits\t:%d\n",stop_bits);
        } else {
            printf("No\n");
        }
        
        
    } else {
        printf("\nStarted.\n");
    }
    
    if(serialRTUenabled) {
        ctx_serial = modbus_new_rtu(tty, baud_rate, parity, length, stop_bits);
        
        if (modbusDebug) {
            modbus_set_debug(ctx_serial, TRUE);
        }
//        modbus_set_debug(ctx_serial, TRUE);
        
        if( -1 == modbus_connect( ctx_serial)) {
            fprintf(stderr, "Connection failed: %s\n",modbus_strerror(errno));
            serialRTUenabled = 0;
            
            /*
                Not sure here.  Should I exit if I have confgured a serial port, or just disable it
             and continue ?
             */
            /*
            if (verbose) {
                printf("\n\tSerial RTU disabled.\n");
            }
            */
            exit(1);
        }
        
        mb_mapping = modbus_mapping_new(
                                        MODBUS_MAX_BITS,   // Coils
                                        MODBUS_MAX_BITS,   // Discrete inputs
                                        MODBUS_MAX_REGISTERS,   // Holding registers
                                        MODBUS_MAX_REGISTERS);  // Input registers
        
        query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
        raw_query = malloc(RAW_REQ_LENGTH);
        raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    }
    
    local_mb_mapping = modbus_mapping_new(
                                          (int) NULL,   // Coils
                                          (int) NULL,   // Discrete inputs
                                          localHoldingRegistersSize,   // Holding registers
                                          localInputRegistersSize);  // Input registers
    
    
    
    /*
     
     Save the existing region pointers, and on shutdown down release the shared memory,
     restore the original pointers and the release the modbuse library allocated structures.
     
     */
    
    if (useSharedHolding) {
        printf("Share the holding regsisters RO\n");
        localHoldingRegisters = local_mb_mapping->tab_registers;
        
        holdingShmKey = iniparser_getint(ini, "system:holding_reg_key",110);
        holdingSemKey = iniparser_getint(ini, "system:holding_reg_sema",111);
        
        holdingShmId = shmget(holdingShmKey, localHoldingRegistersSize, IPC_CREAT | 0600 );  // change to 0400
        
        if (holdingShmId < 0) {
            perror("Holding Reg shmget");
            exit(3);
        }
        local_mb_mapping->tab_registers = (uint16_t *)shmat( holdingShmId,NULL,0);
        if (local_mb_mapping->tab_registers < 0) {
            perror("Holding Reg shmat");
            exit(4);
        }
        
        /* create the seamphore */
        holdingSemId = semget( holdingSemKey, 1, IPC_CREAT | 0600 );
        if (-1 == holdingSemId) {
            perror("Input reg semaphore");
            exit(2);
        }
        /*
         Shared memory created, semaphore created (default is locked)
         release the seamphore !
         */
        rc = semctl(holdingSemId, 0, SETVAL, 1);
    }
    
    if (useSharedInput) {
        printf("Share the input regsisters R/W\n");
        localInputRegisters = local_mb_mapping->tab_input_registers; // Save the current pointer
        inputShmKey = iniparser_getint(ini, "system:input_reg_key",100);
        inputSemKey = iniparser_getint(ini, "system:input_reg_sema",101);
        
        /* Create, attach and place in use the Input registers shared segment */
        inputShmId = shmget( inputShmKey, localInputRegistersSize, IPC_CREAT | 0600 );
        if (inputShmId < 0) {
            perror("Input Reg shmget");
            exit(1);
        }
        local_mb_mapping->tab_input_registers = (uint16_t *)shmat(inputShmId,NULL,0);
        
        if ( local_mb_mapping->tab_input_registers < 0) {
            perror("Input Reg shmat");
            exit(1);
        }
        
        /* create the seamphore */
        inputSemId = semget( inputSemKey, 1, IPC_CREAT | 0600 );
        if (-1 == inputSemId) {
            perror("Input reg semaphore");
            exit(2);
        }
        /*
         Shared memory created, semaphore created (default is locked)
         release the seamphore !
         */
        rc = semctl(inputSemId, 0, SETVAL, 1);
    }
    
    
    local_mb_mapping->tab_input_registers[0]=0xaa55; // Some pattern for testing
    
    ctx_tcp = modbus_new_tcp(ip, port);
    
    if (modbusDebug) {
        modbus_set_debug(ctx_tcp, TRUE);
    }
    
    signal(SIGHUP, sig_handler );
    
    while ( !exitFlag ) {
        header_length = modbus_get_header_length(ctx_tcp);
        
        do {
            socket = modbus_tcp_listen(ctx_tcp, 5);
            if (socket < 0) {
                printf("%04d: Socket in use.\n",cnt++);
                sleep(1);
            }
        } while(socket < 0);
        
        if (socket == -1) {
            printf("\t: %s\n", modbus_strerror(errno));
        }
        
        rc=modbus_tcp_accept(ctx_tcp, &socket);
        
        //        printf("modbus_tcp_accept rc=%d\n",rc);
        if (rc == -1) {
            fprintf(stderr,"FATAL ERROR: %s\n", modbus_strerror(errno));
            exit(-1);
        }
        
        
        while ( runFlag ) {
            do {
                if(verbose) {
                    printf("Recieve.\n");
                }
                rc = modbus_receive(ctx_tcp, query);
            } while (rc == 0);
            
            
            
            if (-1 == rc) {
                if( verbose ) {
                    printf("Client disconnected.\n");
                }
                break;
            }
            
            RTU=query[header_length - 1];
            io_address= ( query[header_length+1] << 8) + (query[header_length+2] & 0xff);
            
            //            printf("IO Address=%04x\n",io_address);
            

            if (verbose) {
                if( localRTU == RTU ) {
                    printf("It's for me !\n");
                } else {
                    printf("RTU Id: %02d\n",RTU);
                }
                
                printf("IO Address=0x%04x\n",io_address);
            }   
            //
            // Need to test for address 0x00, or broadcast.
            //
            if( localRTU == RTU ) {
                //
                // Local mapping
                //
                int len;
                uint16_t data;
                int cnt=0;
                
                
                // Need to add address range checks, and return an error for an invalid function.
                
                switch (query[header_length]) {
                    case 0x03:  // read holding registers
                        if (verbose) {
                            printf("* Read Holding Registers\n");
                        }
                        swab(&query[header_length + 3],&len,2);
                        printf("LEN=%04d\n",len);
                        
                        if (useSharedHolding) {
                            if (verbose) {
                                printf("0x03:LOCK\n");
                            }
                            rc=getSem(holdingSemId);
                        }
                        rc = modbus_reply(ctx_tcp, query, rc, local_mb_mapping);
                        
                        if (useSharedHolding) {
                            if (verbose) {
                                printf("0x03:RELEASE\n");
                            }
                            rc=relSem(holdingSemId);
                        }
                        
                        break;
                    case 0x04:  // read input registers
                        
                        if (verbose) {
                            printf("Read Input Registers\n");
                        }
                        swab(&query[header_length + 3],&len,2);
                        printf("LEN=%04d\n",len);
                        
                        
                        if (localInputRegistersSize < ( io_address + len)*2  ) {
                            printf("0x04:Addressing error exception\n");
                            modbus_reply_exception(ctx_tcp, query,MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);                                                   
                        } else {
                            
                            if (useSharedInput) {
                                if (verbose) {
                                    printf("0x04:LOCK\n");
                                }
                                rc=getSem(inputSemId);
                            }
                            rc = modbus_reply(ctx_tcp, query, rc, local_mb_mapping);
                            
                            if (useSharedInput) {
                                if (verbose) {
                                    printf("0x04:RELEASE\n");
                                }
                                rc=relSem(inputSemId);
                            }
                        }
                        break;
                    case 0x06: //write single register
                        swab(&query[8],&len,2);
                        swab(&query[10],&data,2);
                        printf("Data=0x%0x\n",data);
                        
                        if (localHoldingRegistersSize < ( io_address + len)*2  ) {
                            printf("0x06:Addressing error exception\n");
                            modbus_reply_exception(ctx_tcp, query,MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);                                                   
                        } else {
                            if (useSharedHolding) {
                                if (verbose) {
                                    printf("0x06:LOCK\n");
                                }
                                rc=getSem(holdingSemId);
                            }
                            local_mb_mapping->tab_registers[io_address]=data;
                            rc = modbus_reply(ctx_tcp, query, 12, local_mb_mapping);
                            
                            if (useSharedHolding) {
                                if (verbose) {
                                    printf("0x06:RELEASE\n");
                                }
                                rc=relSem(holdingSemId);
                            }
                        }
                        break;                        
                    case 0x10: // Write multiple registers
                        //                        len=query[7];
                        //                        swab(&query[10],&len,2);
                        
                        len = (query[10] << 8) + (query[11] & 0xff);
                        printf("Length=%02d\n",len);
                        
                        if (useSharedHolding) {
                            if (verbose) {
                                printf("0x10:LOCK\n");
                            }
                            rc=getSem(holdingSemId);
                        }
                        
                        
                        for (i=13; i< (13 + len ); i +=2) {
                            data= (query[i] << 8) + (query[i+1] & 0xff);
                            
                            local_mb_mapping->tab_registers[ io_address + cnt++ ] = data;
                        }
                        rc = modbus_reply(ctx_tcp, query, rc, local_mb_mapping);
                        if (useSharedHolding) {
                            if (verbose) {
                                printf("0x10:RELEASE\n");
                            }
                            rc=relSem(holdingSemId);
                        }
                        break;
                        
                    default:
                        break;
                }
            } else {
                printf("RTU Id: %02d\n",RTU);
                
                if (serialRTUenabled) {
                    handleSerialRTU( query, ctx_serial, mb_mapping);
                } else {
                    if (verbose) {
                        printf("\tSerial RTU disabled.\n");
                    }
                    // And an error response to the client.
                }
                
            }
        }
        
        if( verbose ) {
            printf("Tidying up\n");
        }
        close(socket);
        
        //        usleep( 1000 ); // wait 1 ms
        
    }
    printf("localServer exiting.\n");
    modbus_close(ctx_tcp);
    modbus_free(ctx_tcp);
    free(query);
    modbus_mapping_free(local_mb_mapping);
    return(0);
}
