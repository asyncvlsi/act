#!/bin/sh

ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/prs2net/prs2net"

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
	if [ -f conf_$i ]
	then
		$ACTTOOL -cnf=conf_$i -l -p 'foo<>' $i > runs/$i.stdout 2> runs/$i.stderr
	else
		$ACTTOOL -l -p 'foo<>' $i > runs/$i.stdout 2> runs/$i.stderr
	fi
done
