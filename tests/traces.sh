#!/bin/bash

echo "Testing traces"

if [ ! -x tests/traces.sh ]; then
	echo "Should run from gitroot."
fi

OUTPATH="tests/traces-out"
TESTS_OK=0
TESTS_ERR=0
for a in tests/traces-bin/*; do
	BINNAME=`echo $a | cut -f3 -d'/'`
	FPREFIX=$OUTPATH/$BINNAME
	echo -n Testing: $BINNAME ...

	bin/elf_trace $a 1>$FPREFIX.trace.out 2>$FPREFIX.trace.err
	retval=`grep "Exitcode" $FPREFIX.trace.err | cut -f2`
	if [ -z "$retval" ]; then
		TESTS_ERR=`expr $TESTS_ERR + 1`
		echo "FAILED TO EXECUTE $a."
	else
		TESTS_OK=`expr $TESTS_OK + 1`
		echo OK.
	fi
	echo $retval >$OUTPATH/$BINNAME.trace.ret

#	grep "^[ ]*0x" $OUTPATH/$BINNAME.trace.err >$FPREFIX.trace.addrs
#	for addr in `cat $FPREFIX.trace.addrs`; do
#		objdump -d $a | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
#	done >$FPREFIX.trace.funcs
done
echo "Trace tests done. OK=$TESTS_OK. BAD=$TESTS_ERR"
