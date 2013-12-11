#!/bin/bash

rm -rf guest-test
VEXLLVM_SAVEAS=guest-test pt_run /bin/ls asdd asdasd asdsdasd

if [ ! -e 'guest-test' ]; then echo no snapshot made for ls; exit 1; fi
if [ ! -e 'guest-test/argc' ]; then echo no argc saved; exit 2; fi
if [ ! -e 'guest-test/argv' ]; then echo no argv saved; exit 3; fi

argv_c=`wc -l guest-test/argv | awk ' { print $1 } '`
if [ "$argv_c" != "4" ]; then echo wrong argv count; exit 4; fi

