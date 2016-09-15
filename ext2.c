/*
ext2util.c
===============================================================================
MIT License
Copyright (c) 2007-2016 Michael Lazear

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
===============================================================================

ext2util is a command-line interface for reading/writing data from ext2
disk images. ext2 driver code is directly ported from my own code used in a
hobby operating system. buffer_read and buffer_write are "glue" functions
allowing the ramdisk to emulate a hard disk
*/

#include "ext2.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define NULL ((void*) 0)

/* Leak detector functions */
void* leak_malloc(size_t n, char* file, int line, char* func) {
	void* ptr = malloc(n);
	printf("Allocating %d bytes at 0x%x: %s:%d:%s\n", n, ptr, file, line, func);
	return ptr;
}
void leak_free(void* n, char* file, int line, char* func) {
	printf("Freeing 0x%x %s:%d:%s\n", n, file, line, func);
	free(n);
}

/*
#define malloc(x)	(leak_malloc(x, __FILE__, __LINE__, __func__))
#define free(x)	(leak_free(x, __FILE__, __LINE__, __func__))
*/


static int fp = NULL;

/* Buffer_read and write are used as glue functions for code compatibility 
with hard disk ext2 driver, which has buffer caching functions. Those will
not be included here.  */
buffer* buffer_read(int dev, int block) {
	buffer* b = malloc(sizeof(buffer));
	b->block = block;
	pread(fp, b->data, BLOCK_SIZE, block*BLOCK_SIZE);

	return b;
}

/* Free the buffer block, since we're not caching */
uint32_t buffer_write(buffer* b) {
	assert(b->block);
	pwrite(fp, b->data, BLOCK_SIZE, b->block * BLOCK_SIZE);
	free(b);
}



/* 	Read superblock from device dev, and check the magic flag.
	Return NULL if not a valid EXT2 partition */
superblock* ext2_superblock(int dev) {

	static superblock* sb = NULL;
	assert(dev);
	if(!dev)
		return NULL;
	if (sb)
		return sb;

	buffer* b = buffer_read(dev, EXT2_SUPER);

	sb = malloc(sizeof(superblock));
	memcpy(sb, b->data, sizeof(superblock));
	free(b);

	
	assert(sb->magic == EXT2_MAGIC);
	if (sb->magic != EXT2_MAGIC)
		return NULL;
	assert(1024 << sb->log_block_size == BLOCK_SIZE);
	return sb;
}

/* RW first superblock 
set s to NULL to read a superblock */
superblock* ext2_superblock_rw(int dev, superblock* s) {
	assert(dev);
	if (!dev) 
		return NULL;
	buffer* b = buffer_read(dev, EXT2_SUPER);
	if (!s) {						// Passed a NULL
		s = malloc(sizeof(superblock));
		memcpy(s, b->data, sizeof(superblock));
		free(b);
		return s;
	}
	if (s->magic != EXT2_MAGIC) {	// Non-valid superblock, read 
		memcpy(s, b->data, sizeof(superblock));
		free(b);
	} else {						// Valid superblock, overwrite
		memcpy(b->data, s, sizeof(superblock));
		buffer_write(b);
	}
	
	assert(s->magic == EXT2_MAGIC);
	if (s->magic != EXT2_MAGIC)
		return NULL;
	assert(1024 << s->log_block_size == BLOCK_SIZE);
	return s;
}



block_group_descriptor* ext2_blockdesc(int dev) {
	assert(dev);
	if (!dev) 
		return NULL;

	buffer* b = buffer_read(dev, EXT2_SUPER + 1);
	block_group_descriptor* bg = (block_group_descriptor*) b->data;
	return bg;
}
/* Set bg to NULL to read the block group descriptor
Otherwise, overwrite it */
block_group_descriptor* ext2_blockdesc_rw(int dev, block_group_descriptor* bg, int groupnum) {
	assert(dev);
	if (!dev) 
		return NULL;
	buffer* b = buffer_read(dev, EXT2_SUPER + 1);
	if (bg == NULL) {
		bg = (block_group_descriptor*) b->data;
	} else {
		memcpy(b->data + (sizeof(block_group_descriptor) * groupnum), bg, sizeof(block_group_descriptor));
		buffer_write(b);
	}
	return bg;
}



int ext2_first_free(uint32_t* b, int sz) {
	for (int q = 0; q < sz; q++) {
		uint32_t free_bits = (b[q] ^ ~0x0);
		for (int i = 0; i < 32; i++ ) 
			if (free_bits & (0x1 << i)) 
				return i + (q*32);
	}
	return -1;
}

int ext2_set_bit(uint32_t* b, int bit, int val) {

	b[bit/32] &=  ~(1 << (bit % 32));
	return bit;
}


/* 
Finds a free block from the block descriptor group, and sets it as used
*/
uint32_t ext2_alloc_block(int block_group) {
	block_group_descriptor* bg = ext2_blockdesc(1);
	superblock* s = ext2_superblock(1);

	bg += block_group;
	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap_buf;
	uint32_t* bitmap;
	uint32_t num = 0;
	do {

		bitmap_buf = buffer_read(1, bg->block_bitmap);
		bitmap = malloc(BLOCK_SIZE);
		memcpy(bitmap, bitmap_buf->data, BLOCK_SIZE);

		// Find the first free bit in both bitmaps
		bg++;
		block_group++;
	} while( (num = ext2_first_free(bitmap, BLOCK_SIZE)) == -1);	


	// Should use a macro, not "32"
	bitmap[num / 32] |= (1<<(num % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, BLOCK_SIZE);	
	buffer_write(bitmap_buf);								
	// Free our bitmaps
	free(bitmap);				
	return num + ((block_group - 1) * s->blocks_per_group) + 1;	// 1 indexed				
}



/* 
Finds a free inode from the block descriptor group, and sets it as used
*/
uint32_t ext2_alloc_inode() {
	block_group_descriptor* bg = ext2_blockdesc(1);
	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap_buf = buffer_read(1, bg->inode_bitmap);
	uint32_t* bitmap = malloc(BLOCK_SIZE);
	memcpy(bitmap, bitmap_buf->data, BLOCK_SIZE);

	// Find the first free bit in both bitmaps
	uint32_t num = ext2_first_free(bitmap, BLOCK_SIZE);

	// Should use a macro, not "32"
	bitmap[num / 32] |= (1<<(num % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, BLOCK_SIZE);	
	buffer_write(bitmap_buf);								
	// Free our bitmaps
	free(bitmap);				
	return num + 1;	// 1 indexed				
}
// Converts to same endian-ness as sublime for hex viewing
uint32_t byte_order(uint32_t i) {
	uint32_t x;
	uint8_t* bytes = (uint8_t*) &x;
	bytes[0] = i >> 24 & 0xFF;
	bytes[1] = i >> 16 & 0xFF;
	bytes[2] = i >> 8 & 0xFF;
	bytes[3] = i & 0xFF;
	return x;
}

void ext2_write_indirect(uint32_t indirect, uint32_t link, size_t block_num) {
	buffer* b = buffer_read(1, indirect);
	*(uint32_t*) ((uint32_t) b->data + block_num*4)  = link;
	buffer_write(b);
}

uint32_t ext2_read_indirect(uint32_t indirect, size_t block_num) {
	buffer* b = buffer_read(1, indirect);
	return *(uint32_t*) ((uint32_t) b->data + block_num*4);
}



/* Adds a file to root directory */
int add_to_disk(char* file_name, int i) {
	int fp_add = open(file_name, O_RDWR, 0444);
	assert(fp_add);

	int sz = lseek(fp_add, 0, SEEK_END);		// seek to end of file
	lseek(fp_add, 0, SEEK_SET);		// back to beginning

	char* buffer = malloc(sz);	// File buffer
	pread(fp_add, buffer, sz, 0);
	printf("%s %d\n", file_name, sz);

	if (i) {
		ext2_write_file(i, file_name, buffer, 0x1C0 | EXT2_IFREG, sz);
	} else {
		ext2_touch_file(file_name, buffer, 0x1C0, sz);
	}
	free(buffer);
}


int main(int argc, char* argv[]) {
	static char usage[] = "usage: ext2util -x disk.img [-l] [-wrd] [-i inode | -f fname]";
	extern char *optarg;
	extern int optind;
	int c, err = 0;
	uint32_t flags = 0;

	int inode_num = -1;
	char* file_name = "default_file_name";
	char* image = "default";

	while ( (c = getopt(argc, argv, "lwrdi:f:x:")) != -1) 
		switch(c) {
			case 'x':
				image = optarg;
				flags |= 0x1000;
				break;
			case 'w':
				flags |= 0x1;
				break;
			case 'r':
				flags |= 0x2;
				break;
			case 'd':
				flags |= 0x4;
				break;
			case 'i':
				flags |= 0x20;
				inode_num = atoi(optarg);
				break;
			case 'f':
				flags |= 0x40;
				file_name = optarg;
				break;
			case '?':
				err = 1;
				break;
			case 'l':
				flags |= 0x80;
				break;
		}

	if (err || (flags & 0x1000) == 0) {
		printf("%s\n", usage);
		return;
	}
	if ((flags & 0x3) == 0x3) {
		printf("%s\n", usage);
		printf("Cannot read and write during same run\n");
		return;
	}

	fp = open(image, O_RDWR, 0444);
	assert(fp);
	//printf("Flags %d: %d %s\n", flags, inode_num, file_name );

	superblock* s = ext2_superblock_rw(1, NULL);
	s->mtime = time(NULL);	// Update mount time

	ext2_superblock_rw(1, s);
	sb_dump(s);

	bg_dump(ext2_blockdesc(1));
	


	if (flags & 0x1) {			/* Write */
		if ((flags & 0x60) == 0) {
			printf("%s\n", usage);
			printf("Specify an inode or file name\n");
			return;
		}
		if ((flags & 0x60) == 0x60) {		\
			// Inode & File
			add_to_disk(file_name, inode_num);
		} else if (flags & 0x40) {	
			// Touch a new inode
			add_to_disk(file_name, NULL);
		}
	} else if (flags & 0x2)	{	/* Read */
		//ext2_remove_link(inode_num);
		puts(ext2_read_file(ext2_read_inode(1, inode_num)));
	} 
	if (flags & 0x4) {
		if (flags & 0x20)
			inode_dump(ext2_read_inode(1, inode_num));
	}

	if (flags & 0x80) 
		lsroot();




}
