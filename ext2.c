/*
ext2.c
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
*/
//#include <types.h>
#include "ide.h"
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


FILE *fp = NULL;

buffer* buffer_read(int dev, int block) {
	buffer* b = malloc(sizeof(buffer));
	b->block = block;
	pread(fp, b->data, BLOCK_SIZE, block*BLOCK_SIZE);
	return b;
}
buffer* buffer_write(buffer* b) {}

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
	sb = (superblock*) b->data;
	
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

	if (s->magic != EXT2_MAGIC) {	// Non-valid superblock, read 
		s = (superblock*) b->data;
	} else {							// Valid superblock, overwrite
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

void* ext2_open(inode* in) {
	assert(in);
	if(!in)
		return NULL;
	int num_blocks = in->blocks / (BLOCK_SIZE/SECTOR_SIZE);	
	assert(num_blocks);
	if (!num_blocks){

		return NULL;
	}
	char* buf = malloc(BLOCK_SIZE*num_blocks);

	for (int i = 0; i < num_blocks; i++) {

		buffer* b = buffer_read(1, in->block[i]);
		memcpy((uint32_t)buf+(i*BLOCK_SIZE), b->data, BLOCK_SIZE);
	}

	return buf;
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
	do {
		
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;

		if (d->rec_len != calc && sum == 1024) {
			/* if the calculated value doesn't match the given value,
			then we've reached the final entry on the block */
			sum -= d->rec_len;
	
			d->rec_len = calc; 		// Resize this entry to it's real size
			d = (dirent*)((uint32_t) d + d->rec_len);
			break;

		}
		printf("%d\t/%s\n", d->inode, d->name);

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



int ext2_touch(char* name, char* data, size_t n) {
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
	uint32_t inode_num = ext2_alloc_inode();

	int sz = n;

	if (sz / BLOCK_SIZE > 12) {
		/* NEED INDIRECT */
	}
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

		i->block[q] = block_num;
		i->blocks += 2;			// 2 sectors per block

		/* Go ahead and write the data to disk */
		buffer* b = buffer_read(1, block_num);
		memset(b->data, 0, BLOCK_SIZE);
		memcpy(b->data, (uint32_t) data + (q * BLOCK_SIZE), c);
		buffer_write(b);

		q++;
		sz -= c;	// decrease bytes to write
		s->free_blocks_count--;		// Update Superblock
		bg->free_blocks_count--;	// Update BG
	}

	for (int q = 0; q < i->blocks/2; q++)
		printf("Inode Block[%d] = %d\n", q, i->block[q]);

/* SECTION: INODE ALLOCATION AND DATA WRITING */
	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	int offset 		= (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE;
	bg += block_group;

	//printf("Inode %d:\tGroup %d\tIndex %d\tOffset into table %d\n", inode_num, block_group, index, bg->inode_table+block);
	//printf("Offset %x\n", offset);

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
	s->wtime = time();
	ext2_superblock_rw(1,s);

	/* Update block group descriptors */
	bg->free_inodes_count--;
	ext2_blockdesc_rw(1, bg);

	return inode_num;
}


int main(int argc, char* argv[]) {
		

	fp = open(argv[1], O_RDWR, 0444);

	assert(fp);

	superblock* s = malloc(sizeof(superblock));
	pread(fp, s, sizeof(superblock), 1024);

	sb_dump(s);
	bg_dump(ext2_blockdesc(1));
}