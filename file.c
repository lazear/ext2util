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
	//	ext2_set_bit(bbm->data, blocknum-1, 0);
		bbm->data[(blocknum-1)/32] &= ~( 1 << (blocknum-1) % 32);
	}
	bbm->data[((inode_num-1) % s->inodes_per_group)/32] &= \
		~( 1 << ((inode_num-1) % s->inodes_per_group) % 32);
	 
	//ext2_set_bit(ibm->data, ((inode_num-1) % s->inodes_per_group), 0);
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


/* 
Currently writes data to a new inode 
Need to rewrite this function to handle existing inodes, with overwrite behavior
*/
int ext2_write_file(int inode_num, int parent_dir, char* name, char* data, int mode, uint32_t n) {
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

	inode* i = ext2_read_inode(1, inode_num);

	/* We use creation time as a marker for existing inode */
	if (!i->ctime && !i->blocks) {
		bg->free_inodes_count--;
		s->free_inodes_count--;
	}

	i->mode = mode;		// File
	i->size = n;
	i->atime = time(NULL);
	i->ctime = (i->ctime) ? i->ctime : time(NULL);
	i->mtime = time(NULL);
	i->dtime = 0;
	i->links_count = 1;		/* Setting this to 0 = BIG NO NO */

	int q = 0;		// Block counter
	buffer* indb;
	while(sz) {

		uint32_t block_num = 0;

		/* Do we write BLOCK_SIZE or sz bytes? */
		int c = (sz >= BLOCK_SIZE) ? BLOCK_SIZE : sz;

		if (q < 12) {
			block_num = (i->block[q]) ? i->block[q] : ext2_alloc_block(block_group);
			i->block[q] = block_num;

		} else if (q == 12) {

			block_num = ext2_alloc_block(block_group);
			indirect = block_num;

			indb = buffer_read(1, indirect);
			memset(indb->data, 0, BLOCK_SIZE);
			buffer_write(indb);

			i->block[q] = indirect;
			printf("INDIRECT: %d", indirect);
			//ext2_write_indirect(indirect, block_num, 0);
		} else if(q > 12 && q < ((BLOCK_SIZE/sizeof(uint32_t)) + 12)) {

			//block_num = ext2_read_indirect(indirect, q - 12);
			block_num = ext2_alloc_block(block_group);
			//ext2_write_indirect(indirect, block_num, q - 12);
			ext2_write_indirect(indirect, block_num, q-13);
		}
		
		if (q != 12) {
					/* Go ahead and write the data to disk */
			buffer* b = buffer_read(1, block_num);
			memset(b->data, 0, BLOCK_SIZE);
			memcpy(b->data, (uint32_t) data + (q * BLOCK_SIZE), c);
			buffer_write(b);
			sz -= c;	// decrease bytes to write
					i->blocks += 2;			// 2 sectors per block
		}

		q++;

	}

	if (indirect) {
		ext2_write_indirect(indirect, 0, q-13);
		i->blocks+=2;
	}

	

	/* Mark inode as used in the inode bitmap */
	buffer* b = buffer_read(1, bg->inode_bitmap);
	b->data[index/32] |= (1 << (index % 32));
	buffer_write(b);	

	/* Write inode structure to disk */
	b = buffer_read(1, bg->inode_table+block);
	memcpy((uint32_t) b->data + offset, i, INODE_SIZE);
	buffer_write(b);

	/* Add to parent directory */
	ext2_add_child(parent_dir, inode_num, name, EXT2_FT_REG_FILE);

	/* Update superblock information */
	s->wtime = time(NULL);
	ext2_superblock_rw(1, s);

	/* Update block group descriptors */
	ext2_blockdesc_rw(1, bg, block_group);

	return inode_num;
}

int ext2_touch_file(int parent, char* name, char* data, int mode, size_t n) {
	uint32_t inode_num = ext2_alloc_inode();
	return ext2_write_file(inode_num, parent, name, data, mode | EXT2_IFREG, n);
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
		if (i < 12) {
			blocknum = in->block[i];
			buffer* b = buffer_read(1, blocknum);
			memcpy((uint32_t) buf + (i * BLOCK_SIZE), b->data, BLOCK_SIZE);
		}
		if (i > 12) {
			blocknum = ext2_read_indirect(indirect, i-13);
			buffer* b = buffer_read(1, blocknum);
			memcpy((uint32_t) buf + ((i-1) * BLOCK_SIZE), b->data, BLOCK_SIZE);
		}

		//printf("%x\n", b->data[i]);
	}
	return buf;
}
