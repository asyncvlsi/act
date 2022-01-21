#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
ACTTOOL=../../../../build/act/transform/v2act/v2act

if [ $# -eq 0 ]
then
	list=*.v
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL -l lib.act -n benchlib $i > runs/$i.stdout 2> runs/$i.stderr
done
