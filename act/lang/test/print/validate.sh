#!/bin/sh

ACT="$VLSI_TOOLS_SRC/build/act/lang/test/act-test"

if [ ! -f $ACT ] 
then
	echo "the tool $ACT does not exist"
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
	$ACT -ep $i > runs/$i.stdout 2> runs/$i.stderr
done
