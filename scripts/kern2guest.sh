#!/bin/bash

LINUXIMG=/usr/src/linux/arch/x86/boot/compressed/vmlinux.bin

if [ ! -e "$LINUXIMG" ]; then
	echo could not find $LINUXIMG
	exit 1
fi
cp /usr/src/linux/arch/x86/boot/compressed/vmlinux.bin ./
rm -rf guest-vmlinux
mkdir -p guest-vmlinux/maps/
readelf -eW vmlinux.bin | grep LOAD | while read l; do
	echo $l
	off=`printf "%d" $(echo $l | cut -f2 -d' ')`
	vaddr=`echo $l | cut -f3 -d' '`
	vaddrbegin=`printf "%llu" $vaddr`
	fsz=`printf "%d" $(echo $l | cut -f5 -d' ')`
	msz=`printf "%d" $(echo $l | cut -f6 -d' ')`

	if [ "$vaddrbegin" == "0" ]; then echo 'null vaddr?? skip.'; continue; fi

	dd if=vmlinux.bin of=guest-vmlinux/maps/$vaddr  ibs=1 count=$fsz skip=$off
	slack=`bc <<<"$msz - $fsz"`

	vaddrend=`bc -l <<<"$vaddrbegin + $msz"`
	printf "0x%llx-0x%llx 5 0 [kernel]\n" $vaddr $vaddrend  >>guest-vmlinux/mapinfo

	if [ "$slack" -eq "0" ]; then continue; fi
	dd if=/dev/zero of=guest-vmlinux/maps/$vaddr ibs=1 seek=$fsz count=$slack

done

cp /usr/src/linux/System.map System.map
last_addr=0
last_sym=""
echo Echo processing System.map. Please weight.
grep ffff System.map | while read l; do
	cur_addr=`echo $l | cut -f1 -d' '`
	cur_sym=`echo $l | cut -f3 -d' '`
	if [ ! -z "$last_sym" ]; then
		e=`printf "%llu - 1" 0x$cur_addr`
		end_addr=`bc -l <<<"$e"`
		echo $last_sym $last_addr-`printf "%x" $end_addr`
	fi
	last_addr="$cur_addr"
	last_sym="$cur_sym"
done >guest-vmlinux/syms

echo Done processing System.map...

#x86-64
echo -e -n '\x1\x0\x0\x0' >guest-vmlinux/arch
# can fake most of reg file, but need to have a stack pointer for UC to work
head -c48 /dev/zero >guest-vmlinux/regs
echo -e -n '\x80\xff\xff\x1\x0\x0\x0\x0' >>guest-vmlinux/regs
head -c120 /dev/zero >>guest-vmlinux/regs
# DFLAG must be set or vex will complain
echo -e -n '\x1\x0\x0\x0\x0\x0\x0\x0' >>guest-vmlinux/regs
head -c737 /dev/zero >>guest-vmlinux/regs
# xxx maybe should only have 4k stack to test for stack overflows?
dd if=/dev/zero of=guest-vmlinux/maps/0x1ff8000 bs=4096 count=8
printf "0x%llx-0x%llx 3 0 [stack]\n" 0x1ff8000 0x02000000 >>guest-vmlinux/mapinfo

#dummy fields
touch guest-vmlinux/argv
echo $LINUXIMG >guest-vmlinux/binpath
touch guest-vmlinux/dynsyms
echo -e -n '\x0\x0\x0\x0\x0\x0\x0\x0' >guest-vmlinux/entry
