/*
inode.c - ext2util
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

/* Deal with inode creation and modification */

#include "ext2.h"
#include <stdint.h>
#include <assert.h>


inode* ext2_read_inode(int dev, int i) {
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
	inode* in = malloc(sizeof(inode));
	/* Switched to memcpy. This may avoid issues for OS level buffer caching. */
	memcpy(in, ((uint32_t) b->data + (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE), sizeof(inode));
	
	return in;
}

void ext2_write_inode(int dev, int inode_num, inode* i) {
	superblock* s = ext2_superblock(dev);
	block_group_descriptor* bgd = ext2_blockdesc(dev);

	assert(s->magic == EXT2_MAGIC);
	assert(bgd);

	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	int offset		= (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE;

	bgd += block_group;

	// Not using the inode table was the issue...
	buffer* b = buffer_read(dev, bgd->inode_table+block);
	memcpy((uint32_t) b->data + offset, i, INODE_SIZE);
	buffer_write(b);

}

int ext2_inode_rw(int dev, int inode_num, inode** iptr) {
	superblock* s = ext2_superblock(dev);
	block_group_descriptor* bgd = ext2_blockdesc(dev);

	assert(s->magic == EXT2_MAGIC);
	assert(bgd);
	if (s->magic != EXT2_MAGIC || !bgd)
		return -1;

	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / BLOCK_SIZE; 
	int offset		= (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE;

	bgd += block_group;

	buffer* b = buffer_read(dev, bgd->inode_table+block);
	if (*iptr) {
		memcpy((uint32_t) b->data + offset, *iptr, INODE_SIZE);
		buffer_write(b);
	} else {
		//*iptr = (inode*)((uint32_t) b->data + (index % (BLOCK_SIZE/INODE_SIZE))*INODE_SIZE);
		memcpy(*iptr, ((uint32_t) b->data + offset), sizeof(inode));
	
	}
	return 1;
}