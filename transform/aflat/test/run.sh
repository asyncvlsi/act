#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
ACTTOOL=../aflat.$EXT

check_echo=0
myecho()
{
  if [ $check_echo -eq 0 ]
  then
	check_echo=1
	count=`echo -n "" | wc -c | awk '{print $1}'`
	if [ $count -gt 0 ]
	then
		check_echo=2
	fi
  fi
  if [ $check_echo -eq 1 ]
  then
	echo -n "$@"
  else
	echo "$@\c"
  fi
}


fail=0

if [ ! -d runs ]
then
	mkdir runs
fi

myecho " "
num=0
lim=10
for i in [0-9]*.act
do
	bname=`expr $i : '\(.*\).act'`
	num=`expr $num + 1`
	myecho ".[$bname]"
	$ACTTOOL $i > runs/$i.t.stdout 2> runs/$i.t.stderr
	ok=1
	if ! cmp runs/$i.t.stdout runs/$i.stdout >/dev/null 2>/dev/null
	then
		echo 
		myecho "** FAILED TEST $i: stdout"
		fail=`expr $fail + 1`
		ok=0
	fi
	if ! cmp runs/$i.t.stderr runs/$i.stderr >/dev/null 2>/dev/null
	then
		if [ $ok -eq 1 ]
		then
			echo
			myecho "** FAILED TEST $i:"
		fi
		myecho " stderr"
		fail=`expr $fail + 1`
		ok=0
	fi
	if [ $ok -eq 1 ]
	then
		if [ $num -eq $lim ]
		then
			echo 
			myecho "    "
			num=0
		fi
	else
		echo " **"
		myecho "    "
		num=0
	fi
done

if [ $num -ne 0 ]
then
	echo
fi


if [ $fail -ne 0 ]
then
	if [ $fail -eq 1 ]
	then
		echo "--- Summary: 1 test failed ---"
	else
		echo "--- Summary: $fail tests failed ---"
	fi
	exit 1
else
	echo
	echo "SUCCESS! All tests passed."
fi
