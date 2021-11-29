#!/bin/sh

ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/v2act/v2act"

if [ ! -f $ACTTOOL ] 
then
	echo "the tool $ACTTOOL does not exist"
	exit 1
fi


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
