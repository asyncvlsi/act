#!/bin/sh

ARCH=`$VLSI_TOOLS_SRC/scripts/getarch`
OS=`$VLSI_TOOLS_SRC/scripts/getos`
EXT=${ARCH}_${OS}
if [ ! x$ACT_TEST_INSTALL = x ] || [ ! -f ../act-test.$EXT ]; then
  ACT=$ACT_HOME/bin/act-test
  echo "testing installation"
echo
else
  ACT=../act-test.$EXT
fi

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
count=0
lim=10
while [ -f ${count}.act ]
do
	i=${count}.act
	count=`expr $count + 1`
	bname=`expr $i : '\(.*\).act'`
	num=`expr $num + 1`
	if [ $bname -lt 10 ] 
	then
	   myecho ".[0$bname]"
        else
	   myecho ".[$bname]"
        fi
	$ACT -ep $i > runs/$i.t.stdout 2> runs/$i.t.stderr
	ok=1
	if ! cmp runs/$i.t.stdout runs/$i.stdout >/dev/null 2>/dev/null
	then
		echo 
		myecho "** FAILED TEST $i: stdout"
		fail=`expr $fail + 1`
		ok=0
		if [ ! x$ACT_TEST_VERBOSE = x ]; then
            diff runs/$i.t.stdout runs/$i.stdout
        fi
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
		if [ ! x$ACT_TEST_VERBOSE = x ]; then
            diff runs/$i.t.stderr runs/$i.stderr
        fi
	fi
	if [ $ok -eq 1 ]
	then
		if [ $num -eq $lim ]
		then
			echo 
			myecho " "
			num=0
		fi
	else
		echo " **"
		myecho " "
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
fi
