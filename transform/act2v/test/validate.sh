#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../act2v.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/act2v
  echo "testing installation"
echo
else
  ACTTOOL=../act2v.$EXT
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
	$ACTTOOL -p 'foo<>' $i > runs/$i.stdout 2> runs/$i.stderr
done
