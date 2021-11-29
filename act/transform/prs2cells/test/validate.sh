#!/bin/sh

ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/prs2cells/prs2cells"

if [ ! -f $ACTTOOL ] 
then
	echo "the tool $ACTTOOL does not exist"
	exit 1
fi

if [ $# -eq 0 ]
then
	list=[0-9]*.act
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL $i cells.act runs/$i.cellout > runs/$i.stdout 2> runs/$i.stderr
done
