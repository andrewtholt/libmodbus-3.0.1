#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <sys/socket.h>

/*
 * The number of bytes to allocate for each of the areas.
 */
#define MODBUS_MAX_REGISTERS 0x2000 // 0x1000 16 bit registers
#define MODBUS_MAX_BITS 0x100       // 0x1000 single bits.

#define LOCAL_INPUT_REGISTERS 1024 // 512 RO registers
#define LOCAL_HOLDING_REGISTERS 1024 // 512 RW registers

const uint16_t UT_REGISTERS_NB_SPECIAL = 0x2;

char           *strsave(char *s)
{
  char           *p;
  
  if ((p = (char *) malloc(strlen(s) + 1)) != NULL) {
    strcpy(p, s);
  }
  return (p);
}

void usage() {
  printf("\nUsage: simpleServer\n");
  printf("\t-b <rate>\tSet RTU baud rate.\n");
  printf("\t-h|-?\t\tHelp.\n");
  printf("\t-i <IP Addr>\tIP Address to listen on.\n");
  printf("\t-p <port num>\tNetwork port.\n");
  printf("\t-P E|O|N\tSerial port parity.\n");
  printf("\t-t <port>\tSerial port to use for ModBus RTUs\n");
  
  
  printf("\n");
}
int 
main(int argc, char *argv[])
{
  modbus_t       *ctx_tcp;
  modbus_t    *ctx_serial;
  
  uint8_t        *query;
  uint8_t         *raw_query;
  uint8_t        *raw_reply;
  int             header_length;
  int             socket;
  int rc;
  
  modbus_mapping_t *mb_mapping;
  modbus_mapping_t *local_mb_mapping;
  
  const int RAW_REQ_LENGTH = 10;
  int RTU;
  int exitFlag = 0; // Set to non-zero to exit.
  //    int opt=1;
  int cnt=1;
  
  //    int tty_len=0;
  
  int baud_rate=9600;
  char parity='N';
  int length=8;
  int stop_bits=2;
  char *tty=(char *)NULL;
  char *ip=(char *)NULL;
  int port=1502;
  int verbose=0;
  int io_address=0;
  int tmp_address=0;
  
  uint16_t *localInputRegisters;
  uint16_t *localHoldingRegisters;
  
  int i;
  unsigned char *tmp;
  char ch;    // for getopt
  
  
  tty=strsave("/dev/tty.usbserial-A600drA9");    // default for my mac
  ip=strsave("127.0.0.1");    // default is localhost
  
  while ((ch = getopt(argc,argv,"b:h?i:p:P:t:v")) != -1) {
    switch (ch) {
      case 'n':   // baud rate
	baud_rate=atoi(optarg);
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
      case 'p':
	port=atoi( optarg );
	break;
      case 'P':
	parity=optarg[0];
	
	if ('E' != parity && 'O' != parity && 'N' != parity) {
	  fprintf(stderr,"Invalid parity %c, exiting.\n",parity);
	  exit(-1);
	}
	break;
      case 't':   // serial port
	if (tty != (char *)NULL) {
	  free(tty);
	}
	tty=strsave( optarg );
	break;
      case 'v':
	verbose=1;
	break;
      default:
	break;
    }
  }
  
  localInputRegisters= (uint16_t *)malloc( LOCAL_INPUT_REGISTERS );
  localHoldingRegisters = (uint16_t *)malloc( LOCAL_HOLDING_REGISTERS);
  
  memset( localInputRegisters,0x00, LOCAL_INPUT_REGISTERS);
  memset( localHoldingRegisters,0x00, LOCAL_HOLDING_REGISTERS);
  
  
  if(verbose) {
    printf("\n\t\tSettings\n");
    printf(  "\t\t========\n\n");
    printf("\tBuild Date\t:%s\n",__DATE__);
    printf("\tAddress\t\t:%s\n",ip);
    printf("\tNetwork port\t:%d\n",port);
    printf("\tSerial port\t:%s\n",tty);
    printf("\tBaud rate\t:%d\n",baud_rate);
    printf("\tParity\t\t:%c\n",parity);
  } else {
    printf("\nStarted.\n");
  }
  
  
  query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
  raw_query = malloc(RAW_REQ_LENGTH);
  raw_reply = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
  
  mb_mapping = modbus_mapping_new(
    MODBUS_MAX_BITS,   // Coils
    MODBUS_MAX_BITS,   // Discrete inputs
    MODBUS_MAX_REGISTERS,   // Holding registers
    MODBUS_MAX_REGISTERS);  // Input registers
  
  local_mb_mapping = modbus_mapping_new(
    NULL,   // Coils
    NULL,   // Discrete inputs
    LOCAL_HOLDING_REGISTERS,   // Holding registers
    LOCAL_INPUT_REGISTERS);  // Input registers
  
  
  local_mb_mapping->tab_input_registers[0]=0xaa55;
  
  ctx_tcp = modbus_new_tcp(ip, port);
  
  
  
  ctx_serial = modbus_new_rtu(tty, baud_rate, parity, length, stop_bits);
  
  while ( !exitFlag ) {
    //	ctx_tcp = modbus_new_tcp("127.0.0.1", 1502);
    
    
    header_length = modbus_get_header_length(ctx_tcp);
    
    //        printf("Header length=%d\n", header_length);
    
    if(verbose) {
      modbus_set_debug(ctx_tcp, TRUE);
      //            modbus_set_debug(ctx_serial, TRUE);
    }
    
    do {
      socket = modbus_tcp_listen(ctx_tcp, 5);
      if (socket < 0) {
	printf("%04d: Socket in use.\n",cnt++);
	sleep(1);
      }
    } while(socket < 0);
    
    //        printf("modbus_tcp_listen   =%d\n",socket);
    if (socket == -1) {
      printf("\t: %s\n", modbus_strerror(errno));
    }
    
    rc=modbus_tcp_accept(ctx_tcp, &socket);
    
    //        printf("modbus_tcp_accept rc=%d\n",rc);
    if (rc == -1) {
      fprintf(stderr,"FATAL ERROR: %s\n", modbus_strerror(errno));
      exit(-1);
    }
    
    
    while ( 1 ) {
      do {
	if(verbose) {
	  printf("Recieve.\n");
	}
	rc = modbus_receive(ctx_tcp, query);
	//                printf("rc=%d\n",rc);
	/* Filtered queries return 0 */
      } while (rc == 0);
      
      
      
      if (-1 == rc) {
	if( verbose ) {
	  printf("Client disconnected.\n");
	}
	break;
      }
      RTU=query[header_length - 1];
      io_address= ( query[header_length+1] << 8) + (query[header_length+2] & 0xff);
      
      printf("IO Address=%04x\n",io_address);
      
      
      if (verbose) {
	if( 0xff == RTU ) {
	  printf("It's for me !\n");
	} else {
	  printf("RTU Id: %02d\n",RTU);
	}
	
	printf("IO Address=0x%04x\n",io_address);
      }   
      
      if( 0xff == RTU ) {
	//
	// Local mapping
	//
	int len;
	uint16_t data;
	int cnt=0;
	
	// Need to add address range checks, and return an error for an invalid function.
	
	switch (query[header_length]) {
	  case 0x03:  // read holding registers
	  case 0x04:  // read input registers
	    rc = modbus_reply(ctx_tcp, query, rc, local_mb_mapping);
	    break;
	  case 0x06: //write single register
	    swab(&query[8],&len,2);
	    swab(&query[10],&data,2);
	    printf("Data=%02d\n",data);
	    
	    local_mb_mapping->tab_registers[io_address]=data;
	  case 0x10: // Write multiple registers
	    //                        len=query[7];
	    swab(&query[10],&len,2);
	    printf("Length=%02d\n",len);
	    
	    for (i=13; i< (13 + len ); i +=2) {
	      data= (query[i] << 8) + (query[i+1] & 0xff);
	      printf("%04x\n",data);
	      
	      local_mb_mapping->tab_registers[ io_address + cnt++ ] = data;
	    }
	    
	  default:
	    break;
	}
      } else {
	if( 0x06 == query[header_length] ) {
	  if(verbose) {
	    printf("Write single register.\n");
	    printf("RTU is %d\n",RTU);
	  }
	  modbus_set_slave( ctx_serial,RTU);
	  memcpy(raw_query,&query[header_length-1], 6);
	  rc=modbus_send_raw_request( ctx_serial,raw_query,6);
	  modbus_receive_confirmation(ctx_serial, raw_reply);
	  rc = modbus_reply(ctx_tcp, query, rc, mb_mapping);
	  
	} else if( (0x03 == query[header_length]) || (0x04 == query[header_length])) {
	  int len;
	  
	  if(verbose) {
	    switch (query[header_length]) {
	      case 0x03:
		printf("Read Holding Registers.\n");
		break;
	      case 0x04:
		printf("Read Input Registers.\n");
		break;
	      default:
		break;
	    }
	  }
	  
	  memcpy(raw_query,&query[header_length-1], 6);
	  
	  modbus_set_slave( ctx_serial, query[header_length-1]);
	  
	  if( -1 == modbus_connect( ctx_serial)) {
	    fprintf(stderr, "Connection failed: %s\n",modbus_strerror(errno));
	  }
	  
	  
	  rc=modbus_send_raw_request( ctx_serial,raw_query,6);
	  
	  if( -1 == rc ) {
	    printf("modbus_send_raw_request: %s\n",modbus_strerror(errno));
	  }
	  
	  modbus_receive_confirmation(ctx_serial, raw_reply);
	  
	  /*
	   *                     This next loop swaps bytes in words.
	   *                     If this is built and running on a little endian machine (most machines are these days)
	   *                     Then this needs to be done.  If on a big endian machine (M68000 family) just comment this out.
	   *                     
	   *                     It might be worth having a command line switch.
	   *                     
	   */
	  
	  
	  
	  switch (query[header_length]) {
	    case 0x03:
	      printf("Read Holding Registers.\n");
	      tmp=(unsigned char *)mb_mapping->tab_registers;
	      //                        rc = modbus_reply(ctx_tcp, query, rc, local_mb_mapping);
	      break;
	    case 0x04:
	      printf("Read Input Registers.\n");
	      tmp=(unsigned char *)mb_mapping->tab_input_registers;
	      break;
	    default:
	      break;
	  }
	  
	  tmp_address = (io_address*2);
	  
	  for(i=0;i<raw_reply[2];i=i+2 ) {
	    /*
	     *                         printf("i=%d\n",i);
	     *                         printf("\t%d:i=%d data=%02x\n",i,i+4,raw_reply[i+4]);
	     *                         printf("\t%d:i=%d data=%02x\n",i+1,i+3,raw_reply[i+3]);
	     */
	    tmp[tmp_address+i]=raw_reply[i+4];
	    tmp[tmp_address+i+1]=raw_reply[i+3];
	  }
	  
	  for(i=0;i<10;i++) {
	    if (verbose) {
	      printf("!%02x!",raw_reply[i]);
	    }
	  }
	  printf("\n");
	  
	  for(i=0;i<10;i++) {
	    if (verbose) {
	      printf("+%02x+",tmp[i]);
	    }
	  }
	  printf("\n");
	  
	  //
	  // compute packet length by getting the data length and then add 2.
	  // Then overwrite the outbound data.
	  //
	  len=raw_reply[2] + 3;
	  //                printf("Len=%d\n",len);
	  
	  //                memcpy(  mb_mapping->tab_input_registers, &raw_reply[3], raw_reply[2]);
	  
	  //        MODBUS_SET_INT16_TO_INT8(query, header_length + 3, UT_REGISTERS_NB_SPECIAL - 1);
	  
	}
      }
      
      //    printf("Reply with an invalid TID or slave\n");
      
      //    rc=modbus_send_raw_request(ctx_tcp, raw_reply, 10 * sizeof(uint8_t));
      //    rc=modbus_reply_exception(ctx_tcp, query, 1);
      
      
      rc = modbus_reply(ctx_tcp, query, rc, mb_mapping);
    }
    
    if( verbose ) {
      printf("Tidying up\n");
    }
    close(socket);
    
    usleep( 1000 ); // wait 1 ms
    
  }
  printf("simpleServer exiting.\n");
  modbus_close(ctx_tcp);
  modbus_free(ctx_tcp);
  free(query);
  modbus_mapping_free(mb_mapping);
  return(0);
}
