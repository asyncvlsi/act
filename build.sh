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

if [ x$ACT_HOME = x ]
then
	echo "Environment variable ACT_HOME should point to the install directory"
	exit 1
fi

if [ ! -d build ]; then
	mkdir build
	cd build
	cmake ..
else 
	cd build
fi

cmake --build . -- -j 8
cmake --install . --prefix $ACT_HOME
