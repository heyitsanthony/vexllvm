#!/bin/bash

tests/opcodes/x86_64/gen_optest.sh

for i in tests/opcodes/x86_64/*; do 
	OPCODE=`basename "$i"`
	if [ -d tests/opcodes/x86_64/$OPCODE ]; then
		echo -n "Testing $OPCODE ..."
		for j in tests/opcodes/x86_64/$OPCODE/*; do
			if [ -x $j ]; then
				# skip it if it crashes on its own
				"$j" &> /dev/null
				if [ $? -eq 0 ]; then
					echo -n "."
					bin/pt_xchk "$j" &> /tmp/opcoderesults.log
					if [ $? -ne 0 ]; then
						echo
						echo $j failed
						cat /tmp/opcoderesults.log
					fi
				fi
			fi
		done
	fi
done
