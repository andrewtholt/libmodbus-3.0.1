#!/bin/sh

set -x
rm test
rm .libs/test

echo "  CC    " test.o;gcc -DHAVE_CONFIG_H -I. -I.. -I.. -I../src    -g -O2 -MT test.o -MD -MP -MF .deps/test.Tpo -c -o test.o test.c
mv -f .deps/test.Tpo .deps/test.Po
rm -f test
echo "  CCLD  " test;/bin/sh ../libtool --silent --tag=CC   --mode=link gcc  -g -O2 -o test test.o ../src/libmodbus.la -L/opt/local/lib -lsqlite3
rm -f client

