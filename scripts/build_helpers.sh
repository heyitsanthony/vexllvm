#!/bin/bash

set -eoux pipefail

VEXDIR="$1"
OUTDIR="$2"

cd "$VEXDIR"
./autogen.sh
./configure CFLAGS=-O2 CXXFLAGS=-O2 --prefix=/usr
make -j9

cd VEX
CLANGCMD="clang -emit-llvm -Wall -DHAVE_CONFIG_H -I. -I..  -I.. -I../include -I../VEX/pub -I../VEX/pub -DVGA_amd64=1 -DVGO_linux=1 -DVGP_amd64_linux=1 -DVGPV_amd64_linux_vanilla=1 -Ipriv  -m64 -O2 -g -std=gnu99 -fno-stack-protector -fno-strict-aliasing -fno-builtin  -fomit-frame-pointer -Wbad-function-cast -fstrict-aliasing -O2 -MT "

$CLANGCMD priv/libvex_amd64_linux_a-guest_amd64_helpers.bc -MD -MP -MF priv/.deps/libvex_amd64_linux_a-guest_amd64_helpers.Tpo -c -o priv/libvex_amd64_linux_a-guest_amd64_helpers.bc priv/guest_amd64_helpers.c
$CLANGCMD priv/libvex_amd64_linux_a-guest_generic_x87.bc -MD -MP -MF priv/.deps/libvex_amd64_linux_a-guest_generic_x87.Tpo -c -o priv/libvex_amd64_linux_a-guest_generic_x87.bc priv/guest_generic_x87.c
llvm-link -o libvex_amd64_helpers.debug.bc priv/*.bc
opt -O3 --strip -o libvex_amd64_helpers.bc libvex_amd64_helpers.debug.bc
rm priv/*.bc

$CLANGCMD priv/libvex_amd64_linux_a-guest_x86_helpers.bc -MD -MP -MF priv/.deps/libvex_amd64_linux_a-guest_x86_helpers.Tpo -c -o priv/libvex_amd64_linux_a-guest_x86_helpers.bc priv/guest_x86_helpers.c
$CLANGCMD priv/libvex_amd64_linux_a-guest_generic_x87.bc -MD -MP -MF priv/.deps/libvex_amd64_linux_a-guest_generic_x87.Tpo -c -o priv/libvex_amd64_linux_a-guest_generic_x87.bc priv/guest_generic_x87.c
llvm-link -o libvex_x86_helpers.debug.bc priv/*.bc
opt -O3 --strip -o libvex_x86_helpers.bc libvex_x86_helpers.debug.bc
rm priv/*.bc

$CLANGCMD priv/libvex_amd64_linux_a-guest_arm_helpers.bc -MD -MP -MF priv/.deps/libvex_amd64_linux_a-guest_arm_helpers.Tpo -c -o priv/libvex_amd64_linux_a-guest_arm_helpers.bc priv/guest_arm_helpers.c
opt -O3 --strip -o libvex_arm_helpers.bc priv/*.bc
rm priv/*.bc

mv *helpers.bc "$2"/