#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../prs2net.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/prs2net
  echo "testing installation"
echo
else
  ACTTOOL=../prs2net.$EXT
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
	if [ -f conf_$i ]
	then
		$ACTTOOL -cnf=conf_$i -l -c all.cells -f -p 'foo<>' $i > runs/$i.fstdout 2> runs/$i.fstderr
	else
		$ACTTOOL -l -c all.cells -f -p 'foo<>' $i > runs/$i.fstdout 2> runs/$i.fstderr
	fi
done
