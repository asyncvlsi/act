#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
ACTTOOL=../test_statepass.$EXT 

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
	$ACTTOOL $i 'foo<>' > runs/$i.stdout 2> runs/$i.tmp.stderr
	sort runs/$i.tmp.stderr > runs/$i.stderr
	rm runs/$i.tmp.stderr
done
