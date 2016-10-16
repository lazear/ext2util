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
#include "fs.h"
#include "ext2.h"

#include <stdint.h>
#include <assert.h>


void ext2_read_inode(struct inode* inode) {

	struct super_block* sb = inode->i_sb;
	struct ext2_superblock* s = EXT2_SB(sb);
	

	int block_group = (inode->i_ino - 1) / s->inodes_per_group; // block group #
	int index 		= (inode->i_ino - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * sizeof(struct ext2_inode)) / sb->s_blocksize;
	
	struct ext2_group_desc* bgd = ext2_get_group_desc(sb, block_group);
	// Not using the inode table was the issue...
	struct ide_buffer* b = buffer_read2(sb->s_dev, bgd->inode_table+block, sb->s_blocksize);
	struct ext2_inode* in = (struct ext2_inode*)((uint32_t) b->data + (index % (sb->s_blocksize/INODE_SIZE))*INODE_SIZE);

	memcpy((void*)&inode->u.ext2_i, in, sizeof(struct ext2_inode));
}

void ext2_write_inode(struct inode* inode) {

	struct super_block* sb = inode->i_sb;
	struct ext2_superblock* s = EXT2_SB(sb);

	int block_group = (inode->i_ino - 1) / s->inodes_per_group; // block group #
	int index 		= (inode->i_ino - 1) % s->inodes_per_group; // index into block group
	int block 		= (index * sizeof(struct ext2_inode)) / sb->s_blocksize;

	int offset		= (index % (sb->s_blocksize/INODE_SIZE))*INODE_SIZE;
	struct ext2_group_desc* bgd = ext2_get_group_desc(sb, block_group);

	// Not using the inode table was the issue...
	buffer* b = buffer_read2(sb->s_dev, bgd->inode_table+block, sb->s_blocksize);
	memcpy((uint32_t) b->data + offset, (void*) &inode->u.ext2_i, INODE_SIZE);
	buffer_write2(b);

}

/* 
Finds a free inode from the block descriptor group, and sets it as used
*/
struct inode* ext2_alloc_inode(struct super_block* sb) {
	// Read the block and inode bitmaps from the block descriptor group

	struct ext2_group_desc* bg = ext2_get_group_desc(sb, 0);
	struct ide_buffer* bitmap;
	struct inode* inode = get_empty_inode();
	uint32_t num = 0;

	/* While there are no free inodes, go through the block groups */
	do {

		bitmap = buffer_read2(sb->s_dev, bg->inode_bitmap, sb->s_blocksize);
		bg++;
	} while( (num = ext2_first_free(bitmap->data, sb->s_blocksize)) == -1);	

	// Should use a macro, not "32"
	bitmap->data[num / 32] |= (1<<(num % 32));
	buffer_write2(bitmap);								
	buffer_free(bitmap);		

	inode->i_sb = sb;
	inode->i_ino = num + 1;	// EXT2 inode numbering starts at 1

	return inode;			
}


void ext2_free_inode(struct inode* inode) {
	struct super_block* sb = inode->i_sb;
	struct ext2_superblock* s = EXT2_SB(sb);

	int block_group = (inode->i_ino - 1) / s->inodes_per_group; // block group #
	struct ext2_group_desc* bg = ext2_get_group_desc(sb, block_group);

	// Read the block and inode bitmaps from the block descriptor group
	buffer* bitmap = buffer_read2(sb->s_dev, bg->inode_bitmap, sb->s_blocksize);

	int i_no = inode->i_ino - 1;
	// Should use a macro, not "32"
	bitmap->data[i_no / 32] &= ~(1 << (i_no % 32));
	buffer_write2(bitmap);								
	buffer_free(bitmap);				
}