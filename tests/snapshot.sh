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

test_argc
echo ARGC OK
test_threads
echo THREADS OK
