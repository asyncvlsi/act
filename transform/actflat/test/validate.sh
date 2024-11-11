#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../actflat.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/actflat
  echo "testing installation"
echo
else
  ACTTOOL=../actflat.$EXT
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
	$ACTTOOL -u -p foo -c all.cells $i  > runs/$i.stdout 2> runs/$i.tmp.stderr
	$ACTTOOL -p foo -c all.cells $i  > runs/$i.m.stdout 2> /dev/null
	sort runs/$i.tmp.stderr > runs/$i.stderr
	rm runs/$i.tmp.stderr
done
