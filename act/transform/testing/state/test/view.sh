#!/bin/sh

ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/testing/state/test_statepass"

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
