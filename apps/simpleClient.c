#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>

int main(int argc, char *argv[]) {
    extern int errno;
	modbus_t       *ctx;
	unsigned int    registers;
    unsigned char rtu;
    int addr;
    int nb;
	int             rc;

    addr=0;
    nb=2;
    
//	ctx = modbus_new_tcp("192.168.0.143", 1502);
//	ctx = modbus_new_tcp("127.0.0.1", 1502);
//	ctx = modbus_new_tcp("10.0.0.101", 502);
	ctx = modbus_new_tcp("192.168.100.72", 1502);

	if (ctx == NULL) {
		fprintf(stderr, "modbus_new_tcp failed.\n");
		exit(-1);
	}
	modbus_set_debug(ctx, TRUE);
	modbus_set_error_recovery(ctx,
				  MODBUS_ERROR_RECOVERY_LINK |
				  MODBUS_ERROR_RECOVERY_PROTOCOL);

	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}
    
	rc = modbus_read_registers(ctx, addr, nb,(uint16_t *) &registers);

	if (rc != nb) {
		printf("Failed:%d\n",rc);
        printf("==>%s\n", modbus_strerror(errno));
	}
    
    printf("\nData = 0x%04x\n", registers & 0xffff);

    sleep(1);
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
