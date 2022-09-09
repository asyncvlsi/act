#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../prs2cells.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/prs2cells
  echo "testing installation"
echo
else
  ACTTOOL=../prs2cells.$EXT
fi
if [ $# -eq 0 ]
then
	list=[0-9]*.act
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL $i cells.act runs/$i.cellout > runs/$i.stdout 2> runs/$i.stderr
done
