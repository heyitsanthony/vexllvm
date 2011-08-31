#!/usr/bin/python

# This creates visualization of the hits in the memory map
# In the future, it would be nice to get source-level info from this

import sys
import os
import Image
import ImageColor
import colorsys

class MemEnt:
	def __init__(self, l):
		l = l.replace('-',' ')
		l = l.split(' ')
		self.begin = int(l[0],0)
		self.end = int(l[1],0)
		self.prot = int(l[2])
	def write(self):
		print "%x-%x %d" % (self.begin, self.end, self.prot)

	def is_exec(self):
		return (self.prot & 0x4) != 0

	@staticmethod
	def loadFromFile(fname):
		maptab = dict()
		mapf=open(fname,'r')
		for l in mapf:
			m = MemEnt(l)
			maptab[m.begin] = m
		mapf.close()
		return maptab

def loadFuncList(fname):
	funclist=[]
	funcf=open(fname, 'r')
	for l in funcf:
		if not l.startswith("fn=sb_0x"):
			continue
		addr=l.replace('fn=sb_','')[:-1]
		funclist.append(int(addr, 0))
	funcf.close()
	return funclist

def loadVisitedByFile(fname):
	visited_tab = dict()
	f = open(fname, 'r')
	for l in f:
		# 0xaddress numbytes
		v = l.split(' ')
		visited_tab[int(v[0],0)] = int(v[1])
	f.close()
	return visited_tab

def loadVisitedByRun(guestpath):
	visited_tab = dict()
	for a in funclist:
		s = os.popen('frag_run '+ ('0x%x' % a) +' '+guestpath).read()
		w = filter(lambda x : x.count('VSB') == 1, s.split('\n'))
		if len(w) == 0:
			vsb_sz = '0'
		else:
			fields= w[0].split(' ')
			vsb_sz = (fields[2].split('='))[1]
		print ('0x%x' % a) + " " + vsb_sz
		visited_tab[a] = int(vsb_sz)
	return visited_tab

from optparse import OptionParser
op = OptionParser("usage: %prog istats-file guestpath")
op.add_option(
	'-v',
	'--visited-file',
	dest='visitedFile',
        action="store",
	type="string")
op.add_option(
	'-o',
	'--output-dir',
	dest='outputDir',
	action='store',
	type='string')
opts,args = op.parse_args()

if len(args) != 2:
	op.error("invalid arguments")

guestpath=args[0]
istatspath=args[1]

maptab = MemEnt.loadFromFile(guestpath+"/mapinfo")
funclist = loadFuncList(istatspath)
if opts.visitedFile:
	print "Visit file: " + opts.visitedFile
	visited_tab = loadVisitedByFile(opts.visitedFile)
else:
	visited_tab = loadVisitedByRun(guestpath)


red =  ImageColor.getrgb("red")

def ent2img(visited_tab, m):
	global red

	target_px_w=640
	target_px_h=480
	target_px=target_px_w*target_px_h
	target_dim = (target_px_w, target_px_h)
	m.write()

	# only care about coverage for regions that are exec
	if not m.is_exec():
		return None

	# ignore empty regions, if any
	if m.end - m.begin == 0:
		return None

	pixels_per_byte = float(target_px)/float(m.end - m.begin)
	if pixels_per_byte == 0:
		print "0 pixels per byte. Whoops"
		return	None

	im = Image.new("RGB", target_dim, ImageColor.getrgb("black"))
	for (k, num_bytes) in visited_tab.items():
		if k < m.begin or k+num_bytes > m.end: 
			continue
		offset=int(pixels_per_byte*(k-m.begin))
		for i in range(0, int(num_bytes*pixels_per_byte)):
			x = (offset+i) % target_px_w
			y = (offset+i) / target_px_w
			im.putpixel((x,y), red)
	return im


for m in maptab.values():
	im = ent2img(visited_tab, m)
	if im is None:
		continue
	outfname = ('cov-0x%x' % m.begin) + '.png'
	if opts.outputDir:
		outfname = "%s/%s" % (opts.outputDir,outfname)
	im.save(outfname) 