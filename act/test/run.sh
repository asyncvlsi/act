#!/bin/sh


fail=0
faildirs=""

for i in *
do
	if [ -d $i -a -f $i/0.act -a -f $i/run.sh ]; then
	echo "======= Directory $i ======"
	if (cd $i; ./run.sh)
	then
		:
	else
		fail=`expr $fail + 1`
		faildirs="$i $faildirs"
	fi
	echo "=============================="
	fi
done

if [ $fail -ne 0 ]
then
	echo "** Number of directories that failed: $fail"
	echo "** Directories: $faildirs"
fi
