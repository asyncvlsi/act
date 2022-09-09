#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../ext2sp.$EXT ]; then
  ACTTOOL=$ACT_HOME/bin/ext2sp
  echo "testing installation"
echo
else
  ACTTOOL=../ext2sp.$EXT
fi
if [ $# -eq 0 ]
then
	list=[0-9]*.ext
else
	list="$@"
fi

if [ ! -d runs ]
then
	mkdir runs
fi

for i in $list
do
	$ACTTOOL $i > runs/$i.stdout  2> runs/$i.stderr
done
