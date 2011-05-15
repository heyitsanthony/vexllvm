#!/bin/bash

echo "Testing traces"

if [ ! -x tests/traces.sh ]; then
	echo "Should run from gitroot."
fi

OPROF_SAMPLERATE=1000000

function oprof_start
{
	if [ -z "$TRACES_OPROFILE" ]; then
		return
	fi
	sudo opcontrol --start --vmlinux=/usr/src/linux/vmlinux --event=CPU_CLK_UNHALTED:${OPROF_SAMPLERATE}:0:1:1
}

function oprof_stop
{
	if [ -z "$TRACES_OPROFILE" ]; then
		return
	fi
	sudo opcontrol --dump
	sudo opreport -g -a -s sample --symbols "bin/elf_trace"  >$FPREFIX.oprof
	sudo opcontrol --stop
	sudo opcontrol --reset
}

function oprof_shutdown
{
	if [ -z "$TRACES_OPROFILE" ]; then
		return
	fi

	sudo opcontrol --shutdown
}

function run_trace_bin
{
	oprof_start
	time bin/elf_trace $a 1>"$FPREFIX.trace.out" 2>"$FPREFIX.trace.err"
	oprof_stop
}

function do_trace
{
	BINNAME=`echo $a | sed "s/\//\n/g" | tail -n1`
	FPREFIX=$OUTPATH/$BINNAME
	echo -n "Testing: $BINNAME ..."

	run_trace_bin 2>"$FPREFIX.trace.time"
	retval=`grep "Exitcode" "$FPREFIX.trace.err" | cut -f2`

	echo "$retval" >"${FPREFIX}.trace.ret"
	if [ -z "$retval" ]; then
		TESTS_ERR=`expr $TESTS_ERR + 1`
		echo "FAILED (bin: $a)."
		grep "^[ ]*0x" $OUTPATH/$BINNAME.trace.err >$FPREFIX.trace.addrs
		for addr in `tail -n100 $FPREFIX.trace.addrs`; do
			objdump -d $a | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
		done
	else
		TESTS_OK=`expr $TESTS_OK + 1`
		t=`cat $FPREFIX.trace.time | grep -i real | awk '{ print $2 }' `
		echo "OK.  $t"
	fi

#	grep "^[ ]*0x" $OUTPATH/$BINNAME.trace.err >$FPREFIX.trace.addrs
#	for addr in `cat $FPREFIX.trace.addrs`; do
#		objdump -d $a | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
#	done >$FPREFIX.trace.funcs

}

OUTPATH="tests/traces-out"
TESTS_OK=0
TESTS_ERR=0
echo "Doing built-in tests"
for a in tests/traces-bin/*; do
	do_trace
done

echo "Now testing apps"
while read line
do
	if [ -z "$line" ]; then
		continue
	fi
	a="$line"
	if [ ! -x "$a" ]; then
		continue
	fi
	do_trace 
done < "tests/bin_cmds.txt"

echo "Trace tests done. OK=$TESTS_OK. BAD=$TESTS_ERR"

oprof_shutdown
