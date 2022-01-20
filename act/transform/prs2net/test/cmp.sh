#!/bin/sh

#
# Compare subcircuit netlists by stripping out the device name "M..." 
# and then sorting
#

if [ $# -ne 2 ]
then
	echo "Usage: $0 sp1 sp2"
	exit 1
fi

ckts1=`cat $1 | grep "^\.subckt" | awk '{print $2}'`
ckts2=`cat $2 | grep "^\.subckt" | awk '{print $2}'`

for i in $ckts1
do
	cat $1 | awk "BEGIN { var=0; } /^.subckt $i/ { var=1;} /^.ends/ { print; var=0; } var==1 { print; }" | sed 's/^M[^ ]*//' | sort > __cmp1.sp
	cat $2 | awk "BEGIN { var=0; } /^.subckt $i/ { var=1;} /^.ends/ { print; var=0; } var==1 { print; }" | sed 's/^M[^ ]*//' | sort > __cmp2.sp
	if cmp __cmp1.sp __cmp2.sp
	then
		:
	else
		rm __cmp1.sp __cmp2.sp
		exit 1
	fi
done
rm __cmp1.sp __cmp2.sp
exit 0
