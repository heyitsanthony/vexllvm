#!/bin/bash

#
# analyze stats on different binaries
#
OPTFLAGS="O1 O2 O3 O4 Os g"
if [ -z "$OPTDIR1" ]; then
	OPTDIR1="opt-traces"
fi

if [ -z "$OPTDIR2" ]; then
	OPTDIR2="opt-traces"
fi

if [ -z "$OUTMINED" ]; then
	OUTMINED="opt-traces/mined"
fi

for a in opt-traces/O2/traces-out/*.trace.err; do
	TESTNAME=`echo "$a" | sed "s/\//\n/g" | tail -n1`
	for optfl in $OPTFLAGS; do
		for optfl2 in $OPTFLAGS; do
			if [ $optfl \< $optfl2 ]; then
				continue
			fi
			diff	$OPTDIR1/$optfl/traces-out/$TESTNAME \
				$OPTDIR2/$optfl2/traces-out/$TESTNAME \
				| grep STAT >$OUTMINED/$TESTNAME."$optfl"."$optfl2"
		done
	done
done