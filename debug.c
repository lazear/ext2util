/*
ext2_debug.c
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

void bg_dump(struct ext2_fs* f) {
	printf("Block group descriptor informaton:\n");
	printf("Block bitmap %d\n", f->bg->block_bitmap);
	printf("Inode bitmap %d\n", f->bg->inode_bitmap);
	printf("Inode table  %d\n", f->bg->inode_table);
	printf("Free blocks  %d\n", f->bg->free_blocks_count);
	printf("Free inodes  %d\n", f->bg->free_inodes_count);
	printf("Used dirs    %d\n", f->bg->used_dirs_count);
	
	buffer* bbm = buffer_read(f, f->bg->block_bitmap);
	buffer* ibm = buffer_read(f, f->bg->inode_bitmap);

	printf("First free block: %d\n", ext2_first_free(bbm->data, f->block_size)+1);
	printf("First free inode: %d\n", ext2_first_free(ibm->data, f->block_size)+1);
}



void inode_dump(struct ext2_fs* f, struct ext2_inode* in) {

	printf("Mode\t%x\t", in->mode);			// Format of the file, and access rights
	printf("UID\t%x\t", in->uid);			// User id associated with file
	printf("Size\t%d\n", in->size);			// Size of file in bytes
	printf("Atime\t%x\t", in->atime);			// Last access time, POSIX
	printf("Ctime\t%x\t", in->ctime);			// Creation time
	printf("Mtime\t%x\n", in->mtime);			// Last modified time
	printf("Dtime\t%x\t", in->dtime);			// Deletion time
	printf("Group\t%x\t", in->gid);			// POSIX group access
	printf("#Links\t%d\n", in->links_count);	// How many links
	printf("Sector\t%d\t", in->blocks);		// # of 512-bytes blocks reserved to contain the data
	printf("Flags\t%x\n", in->flags);			// EXT2 behavior
	printf("Blocks:\n");

	for (int i =0; i < in->blocks/(f->block_size / SECTOR_SIZE); i++) {
		if (i <= 12)
			printf("%d ", in->block[i]);		// Block pointers. Last 3 are indirect
		else
			printf("%d ", ext2_read_indirect(f, in->block[12], i-12));
	}
	printf("\n");

}

// For debugging purposes
void sb_dump(struct ext2_superblock* sb) {
	printf("Superblock informaton:\n");
	printf("EXT2 Magic\t%x\n", sb->magic);
	printf("Inodes Count\t%d\t", sb->inodes_count);
	printf("Blocks Count\t%d\t", sb->blocks_count);
	printf("RBlock Count\t%d\n", sb->r_blocks_count);
	printf("FBlock Count\t%d\t", sb->free_blocks_count); 	;
	printf("FInode Count\t%d\t", sb->free_inodes_count);
	printf("First Data Block\t%d\n", sb->first_data_block);
	printf("Block Size\t%d\t", 1024 << sb->log_block_size);
	printf("Fragment Sz \t%d\t", 1024 << sb->log_frag_size);
	printf("Block/Group\t%d\n", sb->blocks_per_group);
	printf("Frag/Group \t%d\t", sb->frags_per_group);
	printf("Inode/Group\t%d\t", sb->inodes_per_group);
	printf("mtime\t%x\n", sb->mtime);
	printf("wtime\t%x\t", sb->wtime);
	printf("Mount #\t%x\t", sb->mnt_count);
	printf("Max mounts\t%x\n", sb->max_mnt_count);
	printf("State\t%x\t", sb->state);
	printf("Errors\t%x\t", sb->errors);
	printf("Minor\t%x\n", sb->minor_rev_level);
	printf("Last Chk\t%x\t", sb->lastcheck);
	printf("Chk int\t%x\t", sb->checkinterval);
	printf("OS\t%x\n", sb->creator_os);
	printf("Rev #\t%x\t", sb->rev_level);
	printf("ResUID\t%x\t", sb->def_resuid);
	printf("ResGID\t%x\n", sb->def_resgid);
}
