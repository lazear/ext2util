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
#include "fs.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>

#define NULL ((void*) 0)
static int fp = NULL;

//#define DEBUG

/* Buffer_read and write are used as glue functions for code compatibility 
with hard disk ext2 driver, which has buffer caching functions. Those will
not be included here.  */
buffer* buffer_read(struct ext2_fs *f, int block) {
	buffer* b = malloc(sizeof(buffer));
	b->block = block;
	b->data = malloc(f->block_size);
	pread(fp, b->data, f->block_size, block*f->block_size);
	#ifdef DEBUG
	printf("Read %d bytes from block %d to buffer %x\n", f->block_size, block, b->data);
	#endif
	return b;
}

buffer* buffer_read2(kdev_t dev, int block, int size) {
	buffer* b = malloc(sizeof(buffer));

	b->block = block;
	b->data = malloc(size);
	pread(fp, b->data, size, block*size);

	return b;
}

/* Free the buffer block, since we're not caching */
uint32_t buffer_write(struct ext2_fs *f, buffer* b) {
	assert(b->block);
	b->flags |= B_DIRTY;	// Dirty

	pwrite(fp, b->data, f->block_size, b->block * f->block_size);
	// IDE handler should clear the flags
	b->flags &= ~B_DIRTY;
	#ifdef DEBUG
	printf("Wrote %d bytes to block %d\n", f->block_size, b->block);
	#endif
}

/* Overloads for superblock read/write, since it is ALWAYS 1024 bytes in
from the beginning of the disk, regardless of logical block size */
buffer* buffer_read_superblock(struct ext2_fs* f) {
	buffer* b = malloc(sizeof(buffer));
	b->block = (f->block_size == 1024) ? 1 : 0;
	b->data = malloc(f->block_size);
	pread(fp, b->data, sizeof(struct ext2_superblock), 1024);
	return b;
}

uint32_t buffer_write_superblock(struct ext2_fs *f, buffer* b) {
	b->flags |= B_DIRTY;	// Dirty
	pwrite(fp, b->data, sizeof(struct ext2_superblock), 1024);
	// IDE handler should clear the flags
	b->flags &= ~B_DIRTY;
}

int buffer_free(buffer* b) {
	// On actual hardware, make sure that the dirty flag has been cleared
	if (b->flags & B_DIRTY)
		return -1;
	#ifdef DEBUG
	printf("Freeing\n");
	#endif
	//free(b->data);

	return 0;
}


/* 	Read superblock from device dev, and check the magic flag.
	Updates the filesystem pointer */
int ext2_superblock_read(struct ext2_fs *f) {
	if (!f)
		return -1;
	if (!f->sb)
		f->sb = malloc(sizeof(struct ext2_superblock));

	buffer* b = buffer_read_superblock(f);
	memcpy(f->sb, b->data, sizeof(struct ext2_superblock));
	buffer_free(b);

	#ifdef DEBUG
	printf("Reading superblock\n");
	#endif
	assert(f->sb->magic == EXT2_MAGIC);
	if (f->sb->magic != EXT2_MAGIC) {
		printf("ABORT: INVALID SUPERBLOCK\n");
		return NULL;
	}
	f->block_size = (1024 << f->sb->log_block_size);
	return 0;
}

/* Sync superblock to disk */
int ext2_superblock_write(struct ext2_fs *f) {

	if (!f)
		return -1;
	if (f->sb->magic != EXT2_MAGIC) {	// Non-valid superblock, read 
		return -1;
	} else {						// Valid superblock, overwrite
		#ifdef DEBUG
		printf("Writing to superblock\n");
		#endif
		buffer* b = buffer_read_superblock(f);
		memcpy(b->data, f->sb, sizeof(struct ext2_superblock));
		buffer_write_superblock(f, b);
		buffer_free(b);
	}
	return 0;
}

int ext2_blockdesc_read(struct ext2_fs *f) {
	if (!f) return -1;

	int num_block_groups = (f->sb->blocks_count / f->sb->blocks_per_group);
	int num_to_read = (num_block_groups * sizeof(struct ext2_block_group_descriptor)) / f->block_size;
	f->num_bg = num_block_groups;
	num_to_read++;	// round up

	#ifdef DEBUG
	printf("Number of block groups: %d (%d blocks)\n", f->num_bg, num_to_read);
	#endif

	if (!f->bg) {
		f->bg = malloc(num_block_groups* sizeof(struct ext2_block_group_descriptor));
	}

	/* Above a certain block size to disk size ratio, we need more than one block */
	for (int i = 0; i < num_to_read; i++) {
		int n = EXT2_SUPER + i + ((f->block_size == 1024) ? 1 : 0); 	
		buffer* b = buffer_read(f, n);
		memcpy((uint32_t) f->bg + (i*f->block_size), b->data, f->block_size);
		buffer_free(b);
	}
	return 0;
}

int ext2_blockdesc_write(struct ext2_fs *f) {
	if (!f) return -1;

	int num_block_groups = (f->sb->blocks_count / f->sb->blocks_per_group);
	int num_to_read = (num_block_groups * sizeof(struct ext2_block_group_descriptor)) / f->block_size;
	/* Above a certain block size to disk size ratio, we need more than one block */
	num_to_read++;	// round up
	for (int i = 0; i < num_to_read; i++) {
		int n = EXT2_SUPER + i + ((f->block_size == 1024) ? 1 : 0); 	
		buffer* b = buffer_read(f, n);
		memcpy(b->data, (uint32_t) f->bg + (i*f->block_size), f->block_size);

		#ifdef DEBUG
		printf("Writing to block group desc (block %d)\n", n);
		#endif
		buffer_write(f, b);
		buffer_free(b);
	}
	return 0;
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

/* 
Finds a free block from the block descriptor group, and sets it as used
*/
uint32_t ext2_alloc_block(struct ext2_fs *f, int block_group) {
	struct ext2_block_group_descriptor* bg = f->bg;
	struct ext2_superblock* s = f->sb;

	bg += block_group;
	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap_buf;
	uint32_t* bitmap = malloc(f->block_size);
	uint32_t num = 0;
	do {

		bitmap_buf = buffer_read(f, bg->block_bitmap);
		memcpy(bitmap, bitmap_buf->data, f->block_size);
		// Find the first free bit in both bitmaps
		bg++;
		block_group++;
	} while( (num = ext2_first_free(bitmap, f->block_size)) == -1);	

	// Should use a macro, not "32"
	bitmap[num / 32] |= (1<<(num % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, f->block_size);	
	buffer_write(f, bitmap_buf);		

	// Free our bitmaps
	free(bitmap);			

	s->free_inodes_count--;
	bg->free_inodes_count--;
	buffer_free(bitmap_buf);
	return num + ((block_group - 1) * s->blocks_per_group) + 1;	// 1 indexed				
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

int ext2_write_indirect(struct ext2_fs *f, uint32_t indirect, uint32_t link, size_t block_num) {
	if (block_num >= (f->block_size / 4))
		return -1;
	buffer* b = buffer_read(f, indirect);
	*(uint32_t*) ((uint32_t) b->data + block_num*4)  = link;
	return buffer_write(f, b);

}

uint32_t ext2_read_indirect(struct ext2_fs *f, uint32_t indirect, size_t block_num) {
	if (block_num >= (f->block_size / 4))
		return NULL;
	buffer* b = buffer_read(f, indirect);
	return *(uint32_t*) ((uint32_t) b->data + block_num*4);
}



/* Adds a file to root directory */
int add_to_disk(struct ext2_fs *f, char* file_name, int i) {
	int fp_add = open(file_name, O_RDWR, 0444);
	assert(fp_add);

	int sz = lseek(fp_add, 0, SEEK_END);		// seek to end of file
	lseek(fp_add, 0, SEEK_SET);		// back to beginning

	char* buffer = malloc(sz);	// File buffer
	int ret = pread(fp_add, buffer, sz, 0);


	if (i) {
		ext2_write_file(f, i, EXT2_ROOTDIR, file_name, buffer, 0x1C0 | EXT2_IFREG, sz);
	} else {
		ext2_touch_file(f, 2, file_name, buffer, 0x1C0, sz);
	}
	printf("%s %d\n", file_name, sz);
	free(buffer);
}


#define F_INODE 	0x20
#define F_WRITE		0x01 
#define F_READ 		0x02 
#define F_DUMP 		0x04 
#define F_FILE 		0x40
#define F_LS 		0x80

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

	gfsp = ext2_mount(1);
	gfsp->sb->mtime = time(NULL);	// Update mount time

	sb_dump(gfsp->sb);



	if (flags & 0x1) {			/* Write */
		if ((flags & 0x60) == 0) {
			printf("%s\n", usage);
			printf("Specify an inode or file name\n");
			return;
		}

			struct stat s;
		stat(file_name, &s);
		printf("%s, %d bytes\n", file_name, s.st_size);

		if ((flags & 0x60) == 0x60) {	
			// Inode & File
			add_to_disk(gfsp, file_name, inode_num);
		} else if (flags & 0x40) {	
			// Touch a new inode
			add_to_disk(gfsp, file_name, NULL);
		}
	} else if (flags & 0x2)	{	/* Read */
		//ext2_remove_link(inode_num);
		if (flags & 0x40) {
			struct ext2_inode* in = ext2_read_inode(gfsp, pathize(gfsp, file_name));
			char* buf = malloc(in->size);
			ext2_read_file(gfsp, in, buf);
			puts(buf);
		}
		else {
			struct ext2_inode* in = ext2_read_inode(gfsp, inode_num);
			char* buf = malloc(in->size);
			ext2_read_file(gfsp, in, buf);
			puts(buf);
		}
	} 
	if (flags & 0x4) {
		if (flags & 0x20)
			inode_dump(gfsp, ext2_read_inode(gfsp, inode_num));
	}

	if (flags & 0x80) 
		ls(gfsp, (flags & F_INODE) ? inode_num : 2);

	return 0;
	//ext2_gen_dirent("New_entry", 5, 1);

}
