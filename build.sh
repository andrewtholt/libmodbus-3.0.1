#!/bin/sh

set -x

CPU=`uname -m`
OS=`uname -s`

echo "building for $CPU"
echo "       OS is $OS"

if [ "$OS" = "Linux" ]; then
    ARCH=$CPU
else
    ARCH="${OS}_${CPU}"
fi

echo 
while getopts a:hx:o: flag; do
    case $flag in
        a)
            ARGS=$OPTARG
            ;;
        h)
            echo "Help."
            printf "\t-a <makefile args>\n"
            printf "\t-h\t\tHelp.\n"
            printf "\t-o <variant>\tBuild a variant based on an architecture\n"
            printf "\t-x <makefile arch>\n"

            exit 0
            ;;
        o)
            OPT=_${OPTARG}
            ;;
        x)
            ARCH=${OPTARG}
            ;;
    esac
done

MAKEFILE=Makefile.${ARCH}${OPT}

if [ -f $MAKEFILE ]; then
    echo "Building with $MAKEFILE"
	make -j 4 -f $MAKEFILE $ARGS
else
	echo "$MAKEFILE does not exist."
fi
