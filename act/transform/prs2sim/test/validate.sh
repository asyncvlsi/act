#!/bin/sh

ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/prs2sim/prs2sim"

if [ ! -f $ACTTOOL ] 
then
	echo "the tool $ACTTOOL does not exist"
	exit 1
fi

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
	$ACTTOOL $i foo 2> runs/$i.stderr
	cat foo.sim foo.al > runs/$i.stdout
        rm foo.sim foo.al
done
