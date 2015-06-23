#!/bin/sh

OBJS=modbus_collect.o modbus_scan.o simpleServer.o simpleClient.o localServer.o
BINS=modbus_collect modbus_scan simpleServer simpleClient localServer
CFLAGS=-Wunused -g -arch i386

INC = /usr/local/include/modbus
LIBS=-L/usr/local/lib -lmodbus -liniparser
SQL=-L/usr/local/lib/i386 -lsqlite3

all:	$(BINS)

modbus_scan:	modbus_scan.o
	$(CC) $(CFLAGS) -o modbus_scan modbus_scan.o $(LIBS) 

modbus_scan.o:	modbus_scan.c
	$(CC) $(CFLAGS) -I$(INC) -c -o modbus_scan.o modbus_scan.c

localServer:	localServer.o
	$(CC) $(CFLAGS) -o localServer localServer.o $(LIBS) 

localServer.o:	localServer.c
	$(CC) $(CFLAGS) -I$(INC) -c -o localServer.o localServer.c

simpleServer:	simpleServer.o
	$(CC) $(CFLAGS) -o simpleServer simpleServer.o $(LIBS) 

simpleServer.o:	simpleServer.c
	$(CC) $(CFLAGS) -I$(INC) -c -o simpleServer.o simpleServer.c

simpleClient:	simpleClient.o
	$(CC) $(CFLAGS) -o simpleClient simpleClient.o $(LIBS) 

simpleClient.o:	simpleClient.c
	$(CC) $(CFLAGS) -I$(INC) -c -o simpleClient.o simpleClient.c

modbus_collect:	modbus_collect.o
	$(CC) $(CFLAGS) -o modbus_collect modbus_collect.o $(SQL) $(LIBS) 

modbus_collect.o:	modbus_collect.c
	$(CC) $(CFLAGS) -I$(INC) -c -o modbus_collect.o modbus_collect.c

helper.o:   helper.c
	$(CC) $(CFLAGS)  -fPIC -c helper.c -o helper.o

libhelper.dylib:    helper.o
	$(CC) $(CFLAGS) -dynamiclib helper.o -o libhelper.dylib

clean:
	rm -f $(OBJS) $(BINS) cscope.out 
