#!/bin/bash

tests/opcodes/x86_64/gen_optest.sh
for i in tests/opcodes/x86_64/*; do 
	OPCODE=`basename "$i"`
	echo "Testing $OPCODE ..."
	for j in tests/opcodes/x86_64/$OPCODE/*; do
		bin/pt_xchk "$j" &> /tmp/opcoderesults.log; 
		if [ $? -ne 0 ]; then 
			echo $j failed
			cat intr.log;
		fi
	done
done