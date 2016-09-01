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
block_group_descriptor* ext2_blockdesc_rw(int dev, block_group_descriptor* bg) {
	assert(dev);
	if (!dev) 
		return NULL;
	buffer* b = buffer_read(dev, EXT2_SUPER + 1);
	if (bg == NULL) {
		bg = (block_group_descriptor*) b->data;
	} else {
		memcpy(b->data, bg, sizeof(block_group_descriptor));
		buffer_write(b);
	}
	return bg;
}

inode* ext2_inode(int dev, int i) {
	superblock* s = ext2_superblock(dev);
	block_group_descriptor* bgd = ext2_blockdesc(dev);

	assert(s->magic == EXT2_MAGIC);
	assert(bgd);

	int block_group = (i - 1) / s->inodes_per_group; // block group #
	int index 		= (i - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	bgd += block_group;

	// Not using the inode table was the issue...
	buffer* b = buffer_read(dev, bgd->inode_table+block);
	inode* in = (inode*)((uint32_t) b->data + (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE);
	
	return in;
}


void ls(dirent* d) {
	do{
	//	d->name[d->name_len] = '\0';
		printf("\t%s\ttype %d\n", d->name, d->file_type);

		d = (dirent*)((uint32_t) d + d->rec_len);
	} while(d->inode);
}

void lsroot() {
	inode* i = ext2_inode(1, 2);			// Root directory

	char* buf = malloc(BLOCK_SIZE*i->blocks/2);
	memset(buf, 0, BLOCK_SIZE*i->blocks/2);

	for (int q = 0; q < i->blocks / 2; q++) {
		buffer* b = buffer_read(1, i->block[q]);
		memcpy((uint32_t)buf+(q * BLOCK_SIZE), b->data, BLOCK_SIZE);
	}

	dirent* d = (dirent*) buf;
	
	int sum = 0;
	int calc = 0;
	printf("ls: /\n");
	do {
		
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;

		if (d->rec_len != calc && sum == 1024) {
			/* if the calculated value doesn't match the given value,
			then we've reached the final entry on the block */
			//sum -= d->rec_len;
	
			d->rec_len = calc; 		// Resize this entry to it's real size
		//	d = (dirent*)((uint32_t) d + d->rec_len);

		}
	printf("%d\t/%s\trec: %d\n", d->inode, d->name, d->rec_len);
	
		d = (dirent*)((uint32_t) d + d->rec_len);


	} while(sum < 1024);
	free(buf);
	return NULL;
}

int ext_first_free(uint32_t* b, int sz) {
	for (int q = 0; q < sz; q++) {
		uint32_t free_bits = (b[q] ^ ~0x0);
		for (int i = 0; i < 32; i++ ) 
			if (free_bits & (0x1 << i)) 
				return i + (q*32);
	}
}


/* 
Finds a free block from the block descriptor group, and sets it as used
*/
uint32_t ext2_alloc_block() {
	block_group_descriptor* bg = ext2_blockdesc(1);
	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap_buf = buffer_read(1, bg->block_bitmap);
	uint32_t* bitmap = malloc(BLOCK_SIZE);
	memcpy(bitmap, bitmap_buf->data, BLOCK_SIZE);

	// Find the first free bit in both bitmaps
	uint32_t num = ext_first_free(bitmap, BLOCK_SIZE);

	// Should use a macro, not "32"
	bitmap[num / 32] |= (1<<(num % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, BLOCK_SIZE);	
	buffer_write(bitmap_buf);								
	// Free our bitmaps
	free(bitmap);				
	return num + 1;	// 1 indexed				
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
	uint32_t num = ext_first_free(bitmap, BLOCK_SIZE);

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

void* ext2_open(inode* in) {
	assert(in);
	if(!in)
		return NULL;

	int num_blocks = in->blocks / (BLOCK_SIZE/SECTOR_SIZE);	

	assert(num_blocks != 0);
	if (!num_blocks) 
		return NULL;


	size_t sz = BLOCK_SIZE*num_blocks;
	void* buf = malloc(sz);
	assert(buf != NULL);

	int indirect = 0;

	/* Singly-indirect block pointer */
	if (num_blocks > 12) {
		indirect = in->block[12];
	}

	int blocknum = 0;
	for (int i = 0; i < num_blocks; i++) {
		if (i < 12) 
			blocknum = in->block[i];
		else
			blocknum = ext2_read_indirect(indirect, i-12);
		if (!blocknum)
			break;
		buffer* b = buffer_read(1, blocknum);
		memcpy((uint32_t) buf + (i * BLOCK_SIZE), b->data, BLOCK_SIZE);
		//printf("%x\n", b->data[i]);
	}
	return buf;
}

int ext2_write_inode(int inode_num, char* name, char* data, size_t n) {
	/* 
	Things we need to do:
		* Find the first free inode # and free blocks needed
		* Mark that inode and block as used
		* Allocate a new inode and fill out the struct
		* Write the new inode to the inode_table
		* Update superblock&blockgroup w/ last mod time, # free inodes, etc
		* Update directory structures
	*/

	superblock* s = ext2_superblock(1);
	block_group_descriptor* bg = ext2_blockdesc(1);
	int sz = n;
	int indirect = 0;

	/* Allocate a new inode and set it up
	*/
	inode* i = malloc(sizeof(inode));
	i->mode = 0x81C0;		// File, User mask
	i->size = n;
	i->atime = time();
	i->ctime = time();
	i->mtime = time();
	i->dtime = 0;
	i->links_count = 1;		/* Setting this to 0 = BIG NO NO */
/* SECTION: BLOCK ALLOCATION AND DATA WRITING */
	int q = 0;

	while(sz) {
		uint32_t block_num = ext2_alloc_block();

		// Do we write BLOCK_SIZE or sz bytes?
		int c = (sz >= BLOCK_SIZE) ? BLOCK_SIZE : sz;

		if (q < 12) {
			i->block[q] = block_num;
		} else if (q == 12) {
			indirect = block_num;
			i->block[q] = indirect;
		//	printf("Indirect block allocated: %d\n", indirect);
			block_num = ext2_alloc_block();
		//	printf("Next data block allocated: %d\n", block_num);
			ext2_write_indirect(indirect, block_num, 0);
		} else if(q > 12 && q < ((BLOCK_SIZE/sizeof(uint32_t)) + 12)) {
			ext2_write_indirect(indirect, block_num, q - 12);
		}

		i->blocks += 2;			// 2 sectors per block


		/* Go ahead and write the data to disk */
		buffer* b = buffer_read(1, block_num);
		memset(b->data, 0, BLOCK_SIZE);
		memcpy(b->data, (uint32_t) data + (q * BLOCK_SIZE), c);
		buffer_write(b);
	//	printf("[%d]\twrite from offset %d to block %d, indirect %d\n", q, q*BLOCK_SIZE, block_num, indirect);
		q++;
		sz -= c;	// decrease bytes to write
		s->free_blocks_count--;		// Update Superblock
		bg->free_blocks_count--;	// Update BG

	}
	if (indirect) {
		ext2_write_indirect(indirect, 0, i->blocks/2);
		i->blocks+=2;
	}
//	printf("Wrote %d blocks to inode %d\n", i->blocks, inode_num);
/*	for (int q = 0; q <= i->blocks/2; q++) {
		if (q < 12)
			printf("Direct   Block[%d] = %d\n", q, i->block[q]);
		else if (q == 12)
			printf("Indirect Block is @= %d\n", i->block[q]);
		else if (q > 12)
			printf("Indirect Block[%d] = %d\n", q - 13, ext2_read_indirect(indirect, q-13));
	}*/


/* SECTION: INODE ALLOCATION AND DATA WRITING */
	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	int offset 		= (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE;
	bg += block_group;

	printf("Inode %d:\tGroup # %d\tIndex # %d\tTable block # %d\n", inode_num, block_group, index, bg->inode_table+block);
	printf("\tOffset into block: %x\n", offset);

	// Not using the inode table was the issue...
	buffer* ib = buffer_read(1, bg->inode_table+block);

	memcpy((uint32_t) ib->data + offset, i, INODE_SIZE);
	buffer_write(ib);

/* SECTION: UPDATE DIRECTORY */
	inode* rootdir = ext2_inode(1, 2);
	buffer* rootdir_buf = buffer_read(1, rootdir->block[0]);

	dirent* d = (dirent*) rootdir_buf->data;
	int sum = 0;
	int calc = 0;
	int new_entry_len = (sizeof(dirent) + strlen(name) + 4) & ~0x3;
	do {
		
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;
		if (d->inode == inode_num)
			break;
		if (d->rec_len != calc && sum == 1024) {
			/* if the calculated value doesn't match the given value,
			then we've reached the final entry on the block */
			sum -= d->rec_len;
			if (sum + new_entry_len > 1024) {
				/* we need to allocate another block for the parent
				directory*/
				printf("PANIC! out of room");
				return NULL;
			}
			d->rec_len = calc; 		// Resize this entry to it's real size
			d = (dirent*)((uint32_t) d + d->rec_len);
			break;

		}
		//printf("name %s\n",d->name);
		d = (dirent*)((uint32_t) d + d->rec_len);


	} while(sum < 1024);

	/* d is now pointing at a blank entry, right after the resized last entry */
	d->rec_len = BLOCK_SIZE - ((uint32_t)d - (uint32_t)rootdir_buf->data);
	d->inode = inode_num;
	d->file_type = 1;
	d->name_len = strlen(name);

	/* memcpy() causes a page fault */
	for (int q = 0; q < strlen(name); q++) 
		d->name[q] = name[q];

	/* Write the buffer to the disk */
	buffer_write(rootdir_buf);

	/* Update superblock information */
	s->free_inodes_count--;
	s->wtime = time(NULL);
	ext2_superblock_rw(1,s);

	/* Update block group descriptors */
	bg->free_inodes_count--;
	ext2_blockdesc_rw(1, bg);

	//free(i);
	return inode_num;
}


int ext2_touch(char* name, char* data, size_t n) {
	uint32_t inode_num = ext2_alloc_inode();
	return ext2_write_inode(inode_num, name, data, n);
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
		ext2_write_inode(i, file_name, buffer, sz);
	} else {
		ext2_touch(file_name, buffer, sz);
	}



	free(buffer);
}


int main(int argc, char* argv[]) {
	static char usage[] = "usage: ext2util -x disk.img [-wrd] [-i inode | -f fname]";
	extern char *optarg;
	extern int optind;
	int c, err = 0;
	uint32_t flags = 0;

	int inode_num = -1;
	char* file_name = "default_file_name";
	char* image = "default";

	while ( (c = getopt(argc, argv, "wrdi:f:x:")) != -1) 
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

	} else if (flags & 0x4) {

	}


}