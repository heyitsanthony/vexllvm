#!/bin/bash

ARCHDIR="tests/opcodes/x86_64"
$ARCHDIR/gen_optest.sh

for i in $ARCHDIR/OP_*; do 
	OPCODE=`basename "$i" | sed "s/OP_//g"`
	if [ ! -d $ARCHDIR/OP_$OPCODE ]; then
		continue
	fi

	echo -n "Testing $OPCODE ..."
	for j in $ARCHDIR/OP_$OPCODE/*; do
		if [ ! -x $j ]; then
			continue
		fi

		"$j" &> /dev/null
		if [ ! $? -eq 0 ]; then
			# skip it if it crashes on its own
			continue
		fi

		echo -n "."
		bin/pt_xchk "$j" &> /tmp/opcoderesults.log
		if [ $? -ne 0 ]; then
			echo
			echo $j failed
			cat /tmp/opcoderesults.log
		fi
	done
	echo
done
