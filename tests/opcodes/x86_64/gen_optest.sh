#!/bin/bash

ARCHDIR="tests/opcodes/x86_64"
FRAGFILE="$ARCHDIR/fragment.c"
OPCFLAGS="-Os"

for op in `cat $ARCHDIR/opcodes.txt`; do
	if [ -e $ARCHDIR/OP_$op ]; then
		continue
	fi
	echo generating $op
	mkdir -p $ARCHDIR/OP_$op
	for r1 in `cat $ARCHDIR/registers.txt`; do
		sed "s/X_UNOP/$op/g;s/REG1/$r1/g" \
			$FRAGFILE >"$ARCHDIR/OP_$op/$r1.c"
		gcc -D_FRAG_UNOP=1 "$ARCHDIR/OP_$op/$r1.c" $OPCFLAGS -o "$ARCHDIR/OP_$op/$r1" \
			>/dev/null 2>&1
		# if [ $? -ne 0 ]; then
		# 	echo "$ARCHDIR/OP_$op/$r1 is bad"
		# 	cat "$ARCHDIR/OP_$op/$r1.c"
		# fi
		for r2 in `cat $ARCHDIR/registers.txt`; do
			sed "s/X_BINOP/$op/g;s/REG1/$r1/g;s/REG2/$r2/g" \
				$FRAGFILE >"$ARCHDIR/OP_$op/$r1-$r2.c"
			gcc -D_FRAG_BINOP=1 "$ARCHDIR/OP_$op/$r1-$r2.c" $OPCFLAGS -o "$ARCHDIR/OP_$op/$r1-$r2" \
				>/dev/null 2>&1
			# if [ $? -ne 0 ]; then
			# 	echo "$ARCHDIR/OP_$op/$r1-$r2 is bad"
			# 	cat "$ARCHDIR/OP_$op/$r1-$r2.c"
			# fi
			for imm in 0 1 2 3 4 5 6 7; do
				sed "s/X_TRIOPIMM/$op/g;s/REG1/$r1/g;s/REG2/$r2/g;s/IMM/$imm/g" \
					$FRAGFILE >"$ARCHDIR/OP_$op/$imm-$r1-$r2.c"
				gcc -D_FRAG_TRIOPI=1 "$ARCHDIR/OP_$op/$imm-$r1-$r2.c" $OPCFLAGS -o "$ARCHDIR/OP_$op/$imm-$r1-$r2" \
					>/dev/null 2>&1
				if [ $? -ne 0 ]; then
				# 	echo "$ARCHDIR/OP_$op/$imm-$r1-$r2 is bad"
				# 	cat "$ARCHDIR/OP_$op/$imm-$r1-$r2.c"
					break
				fi
			done
		done
	done

	rm "$ARCHDIR/OP_$op"/*.c
done
