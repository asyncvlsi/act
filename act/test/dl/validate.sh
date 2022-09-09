#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../act-test.$EXT ]; then
  ACT=$ACT_HOME/bin/act-test
  echo "testing installation"
echo
else
  ACT=../act-test.$EXT
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

make > make.log 2>&1

export LD_LIBRARY_PATH=.:${LD_LIBRARY_PATH}

for i in $list
do
	$ACT -cnf=ext.conf -e $i > runs/$i.stdout 2> runs/$i.stderr
done
