#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../prs2sim.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/prs2sim
  echo "testing installation"
echo
else
  ACTTOOL=../prs2sim.$EXT
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
	$ACTTOOL $i foo 2> runs/$i.stderr
	cat foo.sim foo.al > runs/$i.stdout
        rm foo.sim foo.al
done
