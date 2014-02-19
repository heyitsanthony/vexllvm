#!/bin/bash

sshot="$1"
if [ -z "$sshot" ]; then echo expected snapshot in arg 1; exit 1; fi
if [ ! -d "$sshot" ]; then echo expected snapshot directory; exit 2; fi
arch=`od -An -tx4 "$sshot"/arch | grep 00001`
if [ -z "$arch" ]; then echo only x86-64 supported right now thanks; exit 3; fi

# from /usr/include/valgrind/libvex_guest_amd64.h
rsp=`od -tx8 -An -j48 -N8 "$sshot"/regs | xargs echo`
rip=`od -tx8 -An -j184 -N8 "$sshot"/regs | xargs echo`
stackbegin=`grep "\[stack\]" "$sshot"/mapinfo | cut -f1 -d'-'`

off=$( expr `printf "%ld" 0x$rsp` - `printf "%ld" $stackbegin` )

od -w8 -tx8 -An -N$off "$sshot"/maps/$stackbegin | grep 0 | grep -v  00000000000 | `dirname $0`/addr2sym.py "$sshot"
echo =========
echo 0x"$rip"  | `dirname $0`/addr2sym.py "$sshot"