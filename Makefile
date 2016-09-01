# MIT License
# Copyright (c) 2016 Michael Lazear

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

FINAL	= ext2util
OBJS	= *.o
CC 		= gcc
CCFLAGS = -O -w -std=c99
BCC		= /home/lazear/opt/cross/bin/i686-elf-gcc
BCFLAGS	= -w -fno-builtin -nostdlib -ffreestanding -std=gnu99 -m32 -c 
LD 		= /home/lazear/opt/cross/bin/i686-elf-ld


all: compile 

boot:
	$(BCC) $(BCFLAGS) ext2_bootloader.c
	$(LD) -Ttext 0x1000 -o bootloader ext2_bootloader.o

compile:
	$(CC) $(CCFLAGS) ext2.c ext2_debug.c -o $(FINAL)

new:
	dd if=/dev/zero of=ext2.img bs=1k count=32k
	sudo mke2fs ext2.img
