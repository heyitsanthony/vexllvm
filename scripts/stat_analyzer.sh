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
mkdir -p $OUTMINED

for a in $OPTDIR1/O2/traces-out/*.trace.err; do
	TESTNAME=`echo "$a" | sed "s/\//\n/g" | tail -n1`
	for optfl in $OPTFLAGS; do
		for optfl2 in $OPTFLAGS; do
			if [ $optfl \< $optfl2 ]; then
				continue
			fi
			diff	$OPTDIR1/$optfl/traces-out/$TESTNAME \
				$OPTDIR2/$optfl2/traces-out/$TESTNAME \
				| grep STAT >$OUTMINED/$TESTNAME."$optfl"."$optfl2"

			if [ "$OPTDIR1" != "$OPTDIR2" ]; then
				diff	$OPTDIR1/$optfl2/traces-out/$TESTNAME \
					$OPTDIR2/$optfl/traces-out/$TESTNAME \
					| grep STAT >$OUTMINED/$TESTNAME."$optfl2"."$optfl"
			fi
		done
	done
done

# merged tallies
for optfl in $OPTFLAGS; do
	for optfl2 in $OPTFLAGS; do
		if [ $optfl \< $optfl2 ]; then
			continue
		fi
		
		cut -d' ' -f4 $OUTMINED/*.trace.err."$optfl"."$optfl2" | \
		sort | \
		uniq -c >$OUTMINED/"$optfl"."$optfl2"

		if [ "$OPTDIR1" != "$OPTDIR2" ]; then
			cut -d' ' -f4 $OUTMINED/*.trace.err."$optfl2"."$optfl" | \
			sort | \
			uniq -c >$OUTMINED/"$optfl2"."$optfl"
		fi
	done
done

#finally, summaries
rm -f $OUTMINED/ops.sum
for optfl in $OPTFLAGS; do
	for optfl2 in $OPTFLAGS; do
		if [ $optfl \< $optfl2 ]; then
			continue
		fi
		
		echo "$optfl.$optfl2 " `cut -f2 -d'I' $OUTMINED/"$optfl"."$optfl2" | \
			sort | \
			uniq | \
			wc -l`>>$OUTMINED/ops.sum


		if [ "$OPTDIR1" != "$OPTDIR2" ]; then
			echo "$optfl2.$optfl " `cut -f2 -d'I' $OUTMINED/"$optfl2"."$optfl" | \
				sort | \
				uniq | \
				wc -l`>>$OUTMINED/ops.sum
		fi
	done
done