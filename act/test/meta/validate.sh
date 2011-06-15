#!/bin/sh

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
	../../act -p $i > runs/$i.stdout 2> runs/$i.stderr
done
