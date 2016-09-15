/*
file.c - ext2util
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

#include "ext2.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>


/* Increment the link count of an inode */
void ext2_add_link(int inode_num) {
	inode* in = ext2_read_inode(1, inode_num);
	in->links_count++;
	ext2_write_inode(1, inode_num, in);
	return;
}

/* Check to see if links == 0. If so, begin the process of file deletion,
which consists of:
	* marking the inode and blocks as free 
	* removing inode from root directory
*/
void ext2_remove_link(int inode_num) {
	inode* in = ext2_read_inode(1, inode_num);
	if (--in->links_count) {
		ext2_write_inode(1, inode_num, in);
		return;
	}

	superblock* s = ext2_superblock(1);
	block_group_descriptor* bg = ext2_blockdesc(1);

	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	bg+= block_group;

	int num_blocks 		= in->blocks / 2;
	buffer* bbm = buffer_read(1, bg->block_bitmap);
	buffer* ibm = buffer_read(1, bg->inode_bitmap);
	int indirect = 0;
	int blocknum = 0;

	if (num_blocks > 12) {
		indirect = in->block[12];
	}

	for (int i = 0; i < num_blocks; i++) {
		if (i < 12) {
			blocknum = in->block[i];
			in->block[i] = 0;
		}
		else
			blocknum = ext2_read_indirect(indirect, i-12);
		if (!blocknum)
			break;
		blocknum = blocknum % s->blocks_per_group;
		ext2_set_bit(bbm->data, blocknum-1, 0);
	}
	ext2_set_bit(ibm->data, ((inode_num-1) % s->inodes_per_group), 0);
	buffer_write(ibm);
	buffer_write(bbm);



		/* Update superblock information */
	s->free_inodes_count++;
	s->free_blocks_count += num_blocks;
	s->wtime = time(NULL);
	ext2_superblock_rw(1,s);

	/* Update block group descriptors */
	bg->free_inodes_count++;
	bg->free_blocks_count += num_blocks;
	ext2_blockdesc_rw(1, bg);

	in->block[12] = 0;

	ext2_write_inode(1, inode_num, in);
}


int ext2_write_file(int inode_num, char* name, char* data, int mode, uint32_t n) {
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
	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	int offset 		= (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE;
	
	bg += block_group;
//	printf("Block group: %d\n", block_group);

	/* Allocate a new inode and set it up
	*/
	inode* i = malloc(sizeof(inode));
	i->mode = mode;		// File
	i->size = n;
	i->atime = time();
	i->ctime = time();
	i->mtime = time();
	i->dtime = 0;
	i->links_count = 1;		/* Setting this to 0 = BIG NO NO */
/* SECTION: BLOCK ALLOCATION AND DATA WRITING */
	int q = 0;

	while(sz) {
		uint32_t block_num = ext2_alloc_block(block_group);

		// Do we write BLOCK_SIZE or sz bytes?
		int c = (sz >= BLOCK_SIZE) ? BLOCK_SIZE : sz;

		if (q < 12) {
			i->block[q] = block_num;
		} else if (q == 12) {
			indirect = block_num;
			i->block[q] = indirect;
		//	printf("Indirect block allocated: %d\n", indirect);
			block_num = ext2_alloc_block(block_group);
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


	buffer* bitmap_buf = buffer_read(1, bg->inode_bitmap);
	uint32_t* bitmap = malloc(BLOCK_SIZE);
	memcpy(bitmap, bitmap_buf->data, BLOCK_SIZE);

	bitmap[index/32] |= (1 << (index % 32));
	printf("%x\n", bitmap[index /32]);
	memcpy(bitmap_buf->data, bitmap, BLOCK_SIZE);
	buffer_write(bitmap_buf);	

//	printf("Wrote %d blocks to inode %d\n", i->blocks, inode_num);


/* SECTION: INODE ALLOCATION AND DATA WRITING */

	// Not using the inode table was the issue...
	printf("%d, %d\n", bg->inode_table, block );
	buffer* ib = buffer_read(1, bg->inode_table+block);

	memcpy((uint32_t) ib->data + offset, i, INODE_SIZE);
	buffer_write(ib);

/* SECTION: UPDATE DIRECTORY */
	inode* rootdir = ext2_read_inode(1, 2);
	printf("Root: %d\n", rootdir->block[0]);
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

		if (d->rec_len == 0)
			break;
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
	ext2_blockdesc_rw(1, bg, block_group);

//	printf("(%x): %x\n", bg, sizeof(block_group_descriptor) * block_group);

	//free(i);
	return inode_num;
}

int ext2_touch_file(char* name, char* data, int mode, size_t n) {
	uint32_t inode_num = ext2_alloc_inode();
	return ext2_write_file(inode_num, name, data, mode | EXT2_IFREG, n);
}


void* ext2_read_file(inode* in) {
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