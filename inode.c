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


struct ext2_inode* ext2_read_inode(struct ext2_fs *f, int i) {
	acquire_fs(f);
	struct ext2_superblock* s = f->sb;
	struct ext2_block_group_descriptor* bgd = f->bg;

	int block_group = (i - 1) / s->inodes_per_group; // block group #
	int index 		= (i - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / f->block_size; 

	bgd += block_group;
	// Not using the inode table was the issue...
	buffer* b = buffer_read(f, bgd->inode_table+block);
	struct ext2_inode* in = malloc(INODE_SIZE);
	/* Switched to memcpy. This may avoid issues for OS level buffer caching. */
	memcpy(in, ((uint32_t) b->data + (index % (f->block_size/INODE_SIZE))*INODE_SIZE), INODE_SIZE);
	release_fs(f);
	return in;
}

void ext2_write_inode(struct ext2_fs *f, int inode_num, struct ext2_inode* i) {
	acquire_fs(f);
	struct ext2_superblock* s = f->sb;
	struct ext2_block_group_descriptor* bgd = f->bg;


	int block_group = (inode_num - 1) / s->inodes_per_group; // block group #
	int index 		= (inode_num - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * INODE_SIZE) / f->block_size; 
	int offset		= (index % (f->block_size/INODE_SIZE))*INODE_SIZE;

	bgd += block_group;

	// Not using the inode table was the issue...
	buffer* b = buffer_read(f, bgd->inode_table+block);
	memcpy((uint32_t) b->data + offset, i, INODE_SIZE);
	buffer_write(f, b);

	release_fs(f);

}

/* 
Finds a free inode from the block descriptor group, and sets it as used
*/
uint32_t ext2_alloc_inode(struct ext2_fs *f) {
	// Read the block and inode bitmaps from the block descriptor group

	struct ext2_block_group_descriptor* bg = f->bg;
	buffer* bitmap_buf;
	uint32_t* bitmap = malloc(f->block_size);
	uint32_t num = 0;

	/* While there are no free inodes, go through the block groups */
	do {

		bitmap_buf = buffer_read(f, bg->inode_bitmap);
		memcpy(bitmap, bitmap_buf->data, f->block_size);
		// Find the first free bit in both bitmaps
		bg++;
	} while( (num = ext2_first_free(bitmap, f->block_size)) == -1);	

	// Should use a macro, not "32"
	bitmap[num / 32] |= (1<<(num % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, f->block_size);	
	buffer_write(f, bitmap_buf);								
	// Free our bitmaps
	free(bitmap);	
	buffer_free(bitmap_buf);			
	return num + 1;	// 1 indexed				
}


uint32_t ext2_free_inode(struct ext2_fs *f, int i_no) {
	struct ext2_block_group_descriptor* bg = f->bg;

	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap_buf = buffer_read(f, bg->inode_bitmap);
	uint32_t* bitmap = malloc(f->block_size);
	memcpy(bitmap, bitmap_buf->data, f->block_size);

	i_no -= 1;
	// Should use a macro, not "32"
	bitmap[i_no / 32] &= ~(1 << (i_no % 32));

	// Update bitmaps and write to disk
	memcpy(bitmap_buf->data, bitmap, f->block_size);	
	buffer_write(f, bitmap_buf);								
	// Free our bitmaps
	free(bitmap);				
	return i_no + 1;
}