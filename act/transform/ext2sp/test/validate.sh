#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
ACTTOOL=../../../../build/act/transform/ext2sp/ext2sp

if [ $# -eq 0 ]
then
	list=[0-9]*.ext
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL $i > runs/$i.stdout  2> runs/$i.stderr
done
