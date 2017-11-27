#!/bin/sh

# set -x

OS=`uname -s`

WIN=`uname -s | grep -i cygwin | wc -l`

echo
echo "Running on $OS"

echo "Check for gnuplot ..."

# if [ $WIN -ne 0 ]; then
#     if [ -x "c:/gnuplot/binary/gnuplot.exe" ]; then
#         echo "... found."
#     else
#         echo "... NOT found, plase install gnuplot."
#         echo "... See README"
#         exit 1
#     fi
# else
#     FLAG=`which gnuplot | wc -l`
#     if [ $FLAG -ne 0 ]; then
#         echo "... found."
#     else
#         echo "... NOT found, plase install gnuplot, or fix PATH."
#         exit 1
#     fi
# fi

echo "Checking that default database locations exists ..."

if [ -d /var/data ]; then
    echo " ... Yes it does."
else
    echo " ... No,creating ..."
    mkdir -p /var/data
fi

echo "Checking that destination directories exist ... "
if [ ! -d /usr/local/bin ]; then
    printf "\t... No /usr/local/bin Creating ...\n"
    mkdir -p /usr/local/bin
    printf "\t... done.\n"
fi

echo "Checking that sqlite3 is installed ..."

FLAG=`which sqlite3 | wc -l`

if [ $FLAG -eq 0 ]; then
    echo "... NO.  Install sqlite3 or repair path before re-running this script"
    exit 1
else
    echo " ... Yes it does."
fi

echo "Checking database config script exists and is in current directory ..."
if [ ! -f ./setup.sql ]; then
    echo "./setup.sql not found."
    exit 2
else
    echo " ... Yes it does."
fi

echo "Setting up default database."

if [ -f /var/data/RS.db ]; then
    printf "\tDatabase already exists, replace ? [y/N]:"
    read ans

    if [ -z "$ans" ]; then
        ans="n"
    fi

    if [ "$ans" = "y" ]; then
        printf "\tReplacing database ...\n"
        sqlite3 /var/data/RS.db < ./setup.sql
        printf "\t... done.\n"
    else
        printf "\tPreserving database ...\n"
    fi
else
    echo "No database found, creating ..."
    sqlite3 /var/data/RS.db < ./setup.sql
    echo "... done."
fi

printf "\nCopying executables and data files into place."

LIST="NOTES draw.sh gnuplot.cmds wgnuplot.cmds mkData.sh plot.tcl rs.tcl"

for FILE in $LIST; do
    echo $FILE
    cp $FILE /usr/local/bin
done

DBLIST="setup.sql"
for FILE in $DBLIST; do
    echo $FILE
    cp $FILE /var/data
done
#
#  Copy test, the executable that reads the relays, this is machine specific
#
WINLIST="modbus_scan.exe changePort.sh test.exe libmodbus.lai libmodbus.la libmodbus.dll.a cygmodbus-5.dll"

if [ $WIN -ne 0 ]; then
    for FILE in $WINLIST; do
        echo $FILE
        cp $FILE /usr/local/bin
    done
fi


