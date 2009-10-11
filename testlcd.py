#!/usr/bin/python

f = file('test_lcd.pbm')

fmt = None
size = None
data = ''

# read pbm file (ugly!)
for l in f :
	l = l.strip()
	if l == '' or l[0] == '#' :
		continue

	if fmt == None :
		if l != 'P1' :
			raise RuntimeError('Only P1 ASCII format is supported, not %s.'%(l))
		fmt = l
		continue

	if size==None :
		x,y = map(int,l.split())
		if x != 240 and y != 64 :
			raise RuntimeError('Only 240x64 pixel is supported, not %d x %d.'%(x,y))
		size = (x,y)
		continue

	data = data+l

if len(data) != size[0]*size[1] :
	raise RuntimeError('Too little or too much data read; want %d, got %d.'%(\
		size[0]*size[1],len(data)))

import serial
import sys
import time

S = serial.Serial('/dev/ttyUSB0',115200)
S.write(chr(0x05)) # reset
time.sleep(0.5)    # wait for reset to complete
S.write(chr(0x07)+chr(0x0f)) # display to graphics+text+cursor+blink mode
S.write(chr(0x06)+chr(0x01)) # text+graphics XOR mode
S.write(chr(0x03)+chr(0x40)+chr(0x01)) # set write pointer to 0x0140

for y in range(60) :
	lcddata = ''
	for x in range(0,240,6) :
		d = 0
		for c in range(6) :
			if data[x+y*240+c] == '0' :
				d |= 1<<(5-c)
		lcddata = lcddata + chr(d)
		#S.write(chr(0x01)+chr(d)) # write char
#	print 'Bulk xfer of %d bytes.'%(len(lcddata))
	S.write(chr(0x09)+chr(len(lcddata))+lcddata) # bulk transfer

S.write(chr(0x03)+chr(0)+chr(0x00)) # set write offset
S.write('Hello.')

S.write(chr(0x10)+chr(27)+chr(2)) # cursor pos 27:2
# write at x=10,y=2 -> offs = 2*40+17 = 97
S.write(chr(0x03)+chr(97)+chr(0x00)) # set write offset
S.write('Text_Layer.')

