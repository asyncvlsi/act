#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
ACTTOOL=../../../../../build/act/transform/testing/state/test_statepass

if [ $# -eq 0 ]
then
	list=*.act
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	echo "***** $i *****"
	echo
	cat $i
	echo "--------------"
	cat runs/$i.t.stdout
	echo "--------------"
	cat runs/$i.t.stderr
	echo "*******"
	echo
done
