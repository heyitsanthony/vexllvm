#!/bin/bash

if [ -z "$RUNCMD" ]; then
	echo "RUNCMD not set. Try RUNCMD=bin/pt_trace"
fi


if [ -z "$OUTPATH" ]; then
	echo "OUTPATH not set. Try OUTPATH=tests/traces-out"
fi

TRACE_ADDR_LINES=1000
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
	sudo opreport -g -a -s sample --symbols "$RUNCMD"  >$FPREFIX.oprof
	sudo opcontrol --stop
	sudo opcontrol --reset
}

function run_trace_bin
{
	oprof_start
	time "$RUNCMD" $1 >"$FPREFIX.trace.out" 2>"$FPREFIX.trace.err"
	oprof_stop
}

function run_real_bin
{
	$1 >"$FPREFIX.real.out" 2>"$FPREFIX.real.err"
}

function dump_addrs
{
	grep "^[ ]*0x" $FPREFIX.trace.err >$FPREFIX.trace.addrs
	objdump -d `echo $a | cut -f1 -d' '` >"$FPREFIX".objdump
	for addr in `tail -n$TRACE_ADDR_LINES $FPREFIX.trace.addrs`; do
		cat "$FPREFIX".objdump | grep `echo $addr  | cut -f2 -d'x'` | grep "^0"
	done >$FPREFIX.trace.funcs
	tail $FPREFIX.trace.funcs
}

function do_trace
{
	BINNAME=`echo $a | cut -f1 -d' ' | sed "s/\//\n/g" | tail -n1`
	BINPATH=`echo $a | cut -f1 -d' '`
	CMDHASH=`echo "$a" | md5sum | cut -f1 -d' '`
	FPREFIX=$OUTPATH/$BINNAME.$CMDHASH

	if [ ! -x $BINPATH ]; then
		echo "Skipping: $BINNAME"
		echo "$a">>$OUTPATH/tests.skipped
		return
	fi

	echo -n "Testing: $a..."

	run_trace_bin "$a" 2>"$FPREFIX.trace.time"
	run_real_bin "$a"

	grep -v "VEXLLVM" $FPREFIX.trace.out >$FPREFIX.trace.stripped.out

	retval=`grep "Exitcode" "$FPREFIX.trace.err" | cut -f2`
	assertval=`grep "Assertion"  "$FPREFIX.trace.err" | grep "failed"`
	mismatch=`diff -q -w -B  $FPREFIX.trace.stripped.out $FPREFIX.real.out`

	echo "$retval" >"${FPREFIX}.trace.ret"
	if [ -z "$retval" ] || [ ! -z "$assertval" ]; then
		echo "FAILED (bin: $BINNAME)."
#		dump_addrs
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
