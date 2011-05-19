#!/bin/bash

OUTPATH="tests/traces-out"
echo "Testing traces"

function oprof_shutdown
{
	if [ -z "$TRACES_OPROFILE" ]; then
		return
	fi

	sudo opcontrol --shutdown
}

if [ ! -x tests/traces.sh ]; then
	echo "Should run from gitroot."
fi
rm -f $OUTPATH/tests.ok $OUTPATH/tests.bad
touch $OUTPATH/tests.ok $OUTPATH/tests.bad


echo "Doing built-in tests"
for a in tests/traces-bin/*; do
	tests/trace_docmd.sh "$a"
done

echo "Now testing apps"
IFS=$'\n'
for line in `cat tests/bin_cmds.txt`;
do
	if [ -z "$line" ]; then
		continue
	fi
	a="$line"
	if [ ! -z `echo $a | grep "^#" ` ]; then
		continue
	fi

	binname=`echo "$a" | cut -f1 -d' '` 
	if [ ! -x "$binname" ]; then
		continue
	fi
	tests/trace_docmd.sh "$a"
done

echo "Trace tests done. OK="`wc -l $OUTPATH/tests.ok | cut -f1`". BAD="`wc -l $OUTPATH/tests.bad | cut -f1`

oprof_shutdown
