#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    extern int errno;
	modbus_t       *ctx;
	int registers;
    uint8_t rtu;
    int addr;
    int nb;
	int             rc;
    int ch;
    int verbose=0;
    int debug=0;
    int port=502;
    char name[255];

    addr=0;
    nb=1;

    strcpy(name,"127.0.0.1");
    
//	ctx = modbus_new_tcp("192.168.0.143", 1502);
//	ctx = modbus_new_tcp("127.0.0.1", 1502);
//	ctx = modbus_new_tcp("10.0.0.101", 502);
//	ctx = modbus_new_tcp("192.168.100.72", 1502);

    while((ch = getopt(argc,argv,"dvh?p:i:r:n:c:")) != -1) {
        switch(ch) {
            case 'c':
                sscanf(optarg,"%x",&registers);
                break;
            case 'd':
                debug=1;
                break;
            case 'v':
                verbose=1;
                break;
            case 'h':
            case '?':
                printf("Usage\n");
                exit(1);
                break;
            case 'p':
                port=atoi(optarg);
                break;
            case 'i':
                strcpy(name,optarg);
                break;
            case 'r':
                addr=atoi(optarg);
                break;
            case 'n':
                nb=atoi(optarg);
                break;
        }
    }

    if (verbose) {
        printf("Name    :%s\n",name);
        printf("Port    :%d\n",port);
        printf("Register:%d\n",addr);
        printf("Count   :%d\n", nb);
        printf("Data    :%04x\n", registers);
        printf("==================\n");
    }

	ctx = modbus_new_tcp(name, port);

	if (ctx == NULL) {
		fprintf(stderr, "modbus_new_tcp failed.\n");
		exit(-1);
	}

    if(debug) {
	    modbus_set_debug(ctx, TRUE);
    } else {
	    modbus_set_debug(ctx, FALSE);
    }
	modbus_set_error_recovery(ctx,
				  MODBUS_ERROR_RECOVERY_LINK |
				  MODBUS_ERROR_RECOVERY_PROTOCOL);

	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}
    
	rc = modbus_write_registers(ctx, addr, nb,(uint16_t *) &registers);

	if (rc != nb) {
		printf("Failed:%d\n",rc);
        printf("==>%s\n", modbus_strerror(errno));
	}
    
//    printf("\nData = 0x%04x\n", registers & 0xffff);

    sleep(1);
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
