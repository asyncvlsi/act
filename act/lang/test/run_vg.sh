#!/bin/sh

echo
echo "************************************************************************"
echo "*            Testing core ACT library                                  *"
echo "************************************************************************"
echo


fail=0
faildirs=""

for i in *
do
	if [ -d $i -a -f $i/0.act ]; then
	echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	echo "    Directory $i"
	echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	if [ -f $i/run_vg.sh ]; then
	    if (cd $i; ./run_vg.sh)
	    then
		:
	    else
		fail=`expr $fail + 1`
		faildirs="$i $faildirs"
	    fi
	else
	    if (cd $i; ../run_subdir_vg.sh)
	    then
		:
	    else
		fail=`expr $fail + 1`
		faildirs="$i $faildirs"
	    fi
        fi
       fi
done

if [ $fail -ne 0 ]
then
	echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	echo "** Number of directories that failed: $fail"
	echo "** Directories: $faildirs"
	exit 1
else
	echo
	echo "SUCCESS! All tests passed."
	echo
fi
