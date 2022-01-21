#!/bin/sh

if [ x$VLSI_TOOLS_SRC = x ]
then
	echo "Environment variable VLSI_TOOLS_SRC should point to the source tree"
	exit 1
else
	if [ ! -f $VLSI_TOOLS_SRC/configure ]
	then
	   echo "Environment variable VLSI_TOOLS_SRC should point to the source tree"
	   exit 1
        fi
fi

cmake -B build -S . && cmake --build build && echo && echo 'Run "cmake --install build"'
