#!/bin/sh


ACTTOOL="$VLSI_TOOLS_SRC/build/act/transform/testing/inline/test_inlinepass"

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
	$ACTTOOL $i 'test<>' > runs/$i.stdout 2> runs/$i.tmp.stderr
	sort runs/$i.tmp.stderr > runs/$i.stderr
	rm runs/$i.tmp.stderr
done
