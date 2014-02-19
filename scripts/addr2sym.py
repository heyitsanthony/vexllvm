#!/usr/bin/python

import sys
import os


# symtab is a list sorted by address of (address, func_name, length) tuples
def loadSyms(guestpath):
	symtab = []
	f = open(guestpath + "/syms", 'r')
	# example: xdr_netobj 7f17cac14b70-7f17cac14b81
	for l in f:
		(func_name, addr_range) = l.split(' ')
		(addr_begin, addr_end) = addr_range.split('-')
		addr_begin = int('0x'+addr_begin,0)
		addr_end = int('0x'+addr_end,0)
		symtab.append((addr_begin, func_name, addr_end-addr_begin))
	f.close()
	symtab.sort()
	return symtab

# TODO: Binary search
addr2sym_memo = (0, 'DERP DERP BAD FUNC', 1)
def addr2sym(symtab, addr):
	global addr2sym_memo

	if addr < addr2sym_memo[0]+addr2sym_memo[2] and \
	   addr > addr2sym_memo[0]:
	  	return addr2sym_memo


	last_s = symtab[0]
	for s in symtab:
		if s[0] > addr:
			if addr < (last_s[0]+last_s[2]):
				addr2sym_memo = last_s
				return last_s
			return None
		last_s = s

	if last_s[0] > addr or addr > (last_s[0]+last_s[2]):
		return None

	addr2sym_memo = last_s
	return last_s

if len(sys.argv) < 2:
    print 'Expect snapshot dir as arg1'
    sys.exit(1)



guestpath=sys.argv[1]
symtab = loadSyms(guestpath)

for l in sys.stdin:
    l = l.strip()
    s = addr2sym(symtab, int(l,16))
    if s is None:
        continue
    print l + ": " + s[1]
