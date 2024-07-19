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
	$ACTTOOL -Wno_local_driver:on $i 'foo<>' > runs/$i.stdout 2> runs/$i.tmp.stderr
	sort runs/$i.tmp.stderr > runs/$i.stderr
	rm runs/$i.tmp.stderr
	$ACTTOOL -Wno_local_driver:on -v $i 'foo<>' > runs/$i.stdoutv 2> runs/$i.tmp.stderrv
	sort runs/$i.tmp.stderrv > runs/$i.stderrv
	rm runs/$i.tmp.stderrv
done
