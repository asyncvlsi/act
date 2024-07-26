#!/bin/sh

echo > dummy.cell

for i in [0-9]*.act
do
  read n
  echo "== $i =="
  if prs2cells $i dummy.cell new.cell > /dev/null
  then
     mv new.cell dummy.cell
  fi
done
