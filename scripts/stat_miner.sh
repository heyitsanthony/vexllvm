#!/bin/bash

#
# Use this to compute the opt-traces directory, which will contain the results
# of traces with varying optimization flags
#
if [ -z "$TRACECC" ]; then
	TRACECC="gcc"
fi
	
OPTFLAGS="O1 O2 O3 O4 Os g"
for optflag in $OPTFLAGS; do
	echo $optflag
	mkdir -p tests/traces-{out,bin,obj}
	make tests-clean
	TRACECC="$TRACECC" \
	TRACE_CFLAGS="-$optflag" \
	VEXLLVM_DUMP_XLATESTATS=1 \
		make test-built-traces
	rm -rf opt-traces/"$optflag"
	mkdir -p opt-traces/"$optflag"
	mv tests/traces-{out,bin,obj}  opt-traces/"$optflag"
done