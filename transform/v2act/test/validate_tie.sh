#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../v2act.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/v2act
  echo "testing installation"
echo
else
  ACTTOOL=../v2act.$EXT
fi

if [ $# -eq 0 ]
then
	list=*.v
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL -t -l lib.act -n benchlib $i > runs/t$i.stdout 2> runs/t$i.stderr
done
