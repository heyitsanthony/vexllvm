#!/bin/bash

ARCHDIR="tests/opcodes/x86_64"
OPCFLAGS="-Os"
for op in `cat $ARCHDIR/opcodes.txt`; do
	if [ -e $ARCHDIR/OP_$op ]; then
		continue
	fi
	echo generating $op
	mkdir -p $ARCHDIR/OP_$op
	for r1 in `cat $ARCHDIR/registers.txt`; do
		sed "s/UNOP/$op/g;s/REG1/$r1/g" $ARCHDIR/fragment_unop.c >"$ARCHDIR/OP_$op/$r1.c"
		gcc "$ARCHDIR/OP_$op/$r1.c" $OPCFLAGS -o "$ARCHDIR/OP_$op/$r1" >/dev/null 2>&1
		# if [ $? -ne 0 ]; then
		# 	echo "tests/opcodes/x86_64/$op/$r1 is bad"
		# 	cat "tests/opcodes/x86_64/$op/$r1.c"
		# fi
		for r2 in `cat $ARCHDIR/registers.txt`; do
			sed 	"s/BINOP/$op/g;s/REG1/$r1/g;s/REG2/$r2/g;" \
				$ARCHDIR/fragment_binop.c \
				>"$ARCHDIR/OP_$op/$r1-$r2.c"
			gcc "$ARCHDIR/OP_$op/$r1-$r2.c" $OPCFLAGS \
				-o "$ARCHDIR/OP_$op/$r1-$r2" >/dev/null 2>&1 
		# if [ $? -ne 0 ]; then
		# 	echo "tests/opcodes/x86_64/$op/$r1-$r2 is bad"
		# 	cat "tests/opcodes/x86_64/$op/$r1-$r2.c"
		# fi
		done
	done
	rm -f "$ARCHDIR/OP_$op/*.c"
done
