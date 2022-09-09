#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../test_statepass.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/test_statepass
  echo "testing installation"
echo
else
  ACTTOOL=../test_statepass.$EXT
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
