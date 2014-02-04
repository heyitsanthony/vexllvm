#!/bin/bash

function test_argc
{
echo testing argc
rm -rf guest-test
VEXLLVM_SAVEAS=guest-test pt_run /bin/ls asdd asdasd asdsdasd

if [ ! -e 'guest-test' ]; then echo no snapshot made for ls; exit 1; fi
if [ ! -e 'guest-test/argc' ]; then echo no argc saved; exit 2; fi
if [ ! -e 'guest-test/argv' ]; then echo no argv saved; exit 3; fi

argv_c=`wc -l guest-test/argv | awk ' { print $1 } '`
if [ "$argv_c" != "4" ]; then echo wrong argv count; exit 4; fi
}


function test_threads
{
echo testing threads
rm -rf guest-test
tests/snapshot-bin/threads &
tpid="$!"
sleep 1s
VEXLLVM_ATTACH=$tpid VEXLLVM_SAVEAS=guest-test pt_run none
kill -9 $tpid
if [ ! -e guest-test/threads ]; then echo no thread dir; exit 5; fi
tc=`ls guest-test/threads | wc -l`
if [ "$tc" != 4 ]; then echo wrong thread count; exit 6; fi

diff=`diff guest-test/threads/0 guest-test/threads/1`
if [ -z "$diff" ]; then
	echo threads should have different contexts "(e.g. stacks)"
	exit 7
fi
}

function test_readpost
{
	rm -rf p read-post-chkpts
	mkfifo	p
	tests/snapshot-bin/read-post p &
	rp=$!
	sleep 1s
	mkdir read-post-chkpts
	pushd read-post-chkpts
	echo $rp...
	pgrep read-post
	VEXLLVM_CHKPT_PREPOST=1 ss_chkpt $rp &
	sleep 1s
	echo "0123456789012345678901234567890" >../p
	ok=`od -tx8 chkpt-0001-pre/regs -j16 -N8 -An | xargs printf "%d" | grep 0`
	if [ -z "$ok" ]; then echo Expected sysnr=0; exit 8; fi
	ok=`od -tx8 chkpt-0001-post/regs -j16 -N8 -An | grep 00020`
	if [ -z "$ok" ]; then echo BAD read return value; exit 9; fi
	popd
	sleep 1s
	rm -rf p read-post-chkpts
}

test_argc
echo ARGC OK
test_threads
echo THREADS OK
test_readpost
echo READPOST OK
