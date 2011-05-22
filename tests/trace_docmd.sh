#!/bin/bash

RUNCMD=bin/pt_trace
TRACE_ADDR_LINES=1000
OPROF_SAMPLERATE=1000000
OUTPATH="tests/traces-out"

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
	sudo opreport -g -a -s sample --symbols "$RUNCMD"  >$FPREFIX.oprof
	sudo opcontrol --stop
	sudo opcontrol --reset
}

function run_trace_bin
{
	oprof_start
	time "$RUNCMD" "$1" >"$FPREFIX.trace.out" 2>"$FPREFIX.trace.err"
	oprof_stop
}

function run_real_bin
{
	"$1" >"$FPREFIX.real.out" 2>"$FPREFIX.real.err"
}

function do_trace
{
	BINNAME=`echo $a | cut -f1 -d' ' | sed "s/\//\n/g" | tail -n1`
	FPREFIX=$OUTPATH/$BINNAME
	echo -n "Testing: $BINNAME ..."

	run_trace_bin "$a" 2>"$FPREFIX.trace.time"
	run_real_bin "$a"

	grep -v "VEXLLVM" $FPREFIX.trace.out >$FPREFIX.trace.stripped.out

	retval=`grep "Exitcode" "$FPREFIX.trace.err" | cut -f2`
	assertval=`grep "Assertion"  "$FPREFIX.trace.err" | grep "failed"`
	mismatch=`diff -q -w -B  $FPREFIX.trace.stripped.out $FPREFIX.real.out`

	echo "$retval" >"${FPREFIX}.trace.ret"
	if [ -z "$retval" ] || [ ! -z "$assertval" ]; then
		echo "FAILED (bin: $BINNAME)."
		grep "^[ ]*0x" $OUTPATH/$BINNAME.trace.err >$FPREFIX.trace.addrs
		objdump -d `echo $a | cut -f1 -d' '` >"$FPREFIX".objdump
		for addr in `tail -n$TRACE_ADDR_LINES $FPREFIX.trace.addrs`; do
			cat "$FPREFIX".objdump | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
		done >$FPREFIX.trace.funcs
		tail $FPREFIX.trace.funcs

		echo "$a">>$OUTPATH/tests.bad
	else
		t=`cat $FPREFIX.trace.time | grep -i real | awk '{ print $2 }' `
		echo -n  "OK.  $t"
		echo "$a">>$OUTPATH/tests.ok

		if [ ! -z "$mismatch" ]; then
			echo " (mismatched)"
			echo "$a">>$OUTPATH/tests.mismatch
		else
			echo ""
		fi
	fi
}

a="$1"
do_trace
