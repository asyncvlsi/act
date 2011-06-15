#!/bin/sh

if [ $# -eq 0 ]
then
	list=*.act
else
	list="$@"
fi

for i in $list
do
	act-test $i > runs/$i.stdout 2> runs/$i.stderr
done
