/*
dir.c - ext2util
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
#include <assert.h>


/* Add an inode into parent_inode */
int ext2_add_child(int parent_inode, int i_no, char* name, int type) {

	inode* rootdir = ext2_read_inode(1, parent_inode);
	assert(rootdir->blocks);
	assert(rootdir->block[0]);

	buffer* rootdir_buf = buffer_read(1, rootdir->block[0]);
	dirent* d = (dirent*) rootdir_buf->data;

	int sum = 0;
	int calc = 0;
	int new_entry_len = (sizeof(dirent) + strlen(name) + 4) & ~0x3;
	do {
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;

		/* Child already exists here */
		if (d->inode == i_no)
			return -1;

		if (strcmp(d->name, name) == 0)
			return -1;

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
	d->rec_len 		= BLOCK_SIZE - ((uint32_t)d - (uint32_t)rootdir_buf->data);
	d->inode 		= i_no;
	d->file_type 	= 1;
	d->name_len 	= strlen(name);

	for (int q = 0; q < strlen(name); q++) 
		d->name[q] = name[q];

	/* Write the buffer to the disk */
	buffer_write(rootdir_buf);

	return 1;
}


/* Create a new directory
Need to handle cases where this entry causes an overflow of the parent directory,
i.e. force allocation of a new block */
dirent* ext2_create_dir(char* name, int parent_inode) {
	int mode = EXT2_IFDIR | EXT2_IRUSR | EXT2_IWUSR | EXT2_IXUSR;
	int i_no = ext2_alloc_inode();
	//int block_group = (i_no - 1) / s->inodes_per_group; // block group #

	inode* in = malloc(sizeof(inode)); 
	in->blocks = 2;
	puts("DEBUG1");
	in->block[0] = ext2_alloc_block(1);
	puts("DEBUG2");
	in->mode = mode;	
	in->size = 0;
	in->atime = time();
	in->ctime = time();
	in->mtime = time();
	in->dtime = 0;
	in->links_count = 1;		/* Setting this to 0 = BIG NO NO */


	ext2_write_inode(1, i_no, in);
	assert(ext2_add_child(parent_inode, i_no, name, EXT2_FT_DIR));

	return i_no;
}


void ls(dirent* d) {
	do{
	//	d->name[d->name_len] = '\0';
		printf("\t%s\ttype %d\n", d->name, d->file_type);

		d = (dirent*)((uint32_t) d + d->rec_len);
	} while(d->inode);
}

void lsroot() {
	inode* i = ext2_read_inode(1, 2);		// Parent directory
	char* buf = malloc(BLOCK_SIZE*i->blocks/2);

	memset(buf, 0, BLOCK_SIZE*i->blocks/2);

	for (int q = 0; q < i->blocks / 2; q++) {
		printf("block %d\n", i->block[q]);
		buffer* b = buffer_read(1, i->block[q]);
		memcpy((uint32_t)buf+(q * BLOCK_SIZE), b->data, BLOCK_SIZE);
	}
	assert(i->blocks);
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
	
		//	d->rec_len = calc; 		// Resize this entry to it's real size
		//	d = (dirent*)((uint32_t) d + d->rec_len);

		}
		if (d->rec_len)
			printf("%d\t/%s\trec: %d\n", d->inode, d->name, d->rec_len);
	
		d = (dirent*)((uint32_t) d + d->rec_len);


	} while(sum < 1024);
	free(buf);
	return NULL;
}