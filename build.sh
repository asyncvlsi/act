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

echo "Building common library/generators..." && \
   make -C common install_inc && \
   make -C common install && \
   make -C pgen install && \
   make -C miniscm install 

make && echo && echo 'Run "make install" to install the binaries'
