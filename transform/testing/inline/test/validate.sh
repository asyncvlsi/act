#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../test_inlinepass.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/test_inlinepass
  echo "testing installation"
echo
else
  ACTTOOL=../test_inlinepass.$EXT
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
	$ACTTOOL $i 'test<>' > runs/$i.stdout 2> runs/$i.tmp.stderr
	sort runs/$i.tmp.stderr > runs/$i.stderr
	rm runs/$i.tmp.stderr
done
