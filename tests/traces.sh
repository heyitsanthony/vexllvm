#!/bin/bash

TRACE_ADDR_LINES=1000

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
	time bin/elf_trace $1 1>"$FPREFIX.trace.out" 2>"$FPREFIX.trace.err"
	oprof_stop
}

function do_trace
{
	BINNAME=`echo $a | cut -f1 -d' ' | sed "s/\//\n/g" | tail -n1`
	FPREFIX=$OUTPATH/$BINNAME
	echo -n "Testing: $BINNAME ..."

	run_trace_bin "$a" 2>"$FPREFIX.trace.time"
	retval=`grep "Exitcode" "$FPREFIX.trace.err" | cut -f2`

	echo "$retval" >"${FPREFIX}.trace.ret"
	if [ -z "$retval" ]; then
		TESTS_ERR=`expr $TESTS_ERR + 1`
		echo "FAILED (bin: $BINNAME)."
		grep "^[ ]*0x" $OUTPATH/$BINNAME.trace.err >$FPREFIX.trace.addrs
		objdump -d `echo $a | cut -f1 -d' '` >"$FPREFIX".objdump
		for addr in `tail -n$TRACE_ADDR_LINES $FPREFIX.trace.addrs`; do
			cat "$FPREFIX".objdump | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
		done >$FPREFIX.trace.funcs
		tail $FPREFIX.trace.funcs

		echo "$a">>$OUTPATH/tests.bad
	else
		TESTS_OK=`expr $TESTS_OK + 1`
		t=`cat $FPREFIX.trace.time | grep -i real | awk '{ print $2 }' `
		echo "OK.  $t"
		echo "$a">>$OUTPATH/tests.ok
	fi

}

OUTPATH="tests/traces-out"
TESTS_OK=0
TESTS_ERR=0
rm -f $OUTPATH/tests.ok $OUTPATH/tests.bad

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
	if [ ! -z `echo $a | grep "^#" ` ]; then
		continue
	fi

	binname=`echo "$a" | cut -f1 -d' '` 
	if [ ! -x "$binname" ]; then
		continue
	fi

	do_trace 
done < "tests/bin_cmds.txt"

echo "Trace tests done. OK=$TESTS_OK. BAD=$TESTS_ERR"

oprof_shutdown
