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

/* Create a new directory
Need to handle cases where this entry causes an overflow of the parent directory,
i.e. force allocation of a new block */
dirent* ext2_create_dir(char* name, int parent_inode) {
	int mode = EXT2_IFDIR | EXT2_IRUSR | EXT2_IWUSR | EXT2_IXUSR;
	int n = 0;

	dirent* this = malloc(sizeof(dirent));					// our new directory entry
	buffer* b;
	inode* i = ext2_read_inode(1, parent_inode);		// Parent directory
	char* data;
	b = buffer_read(1, i->block[i->blocks/2 - 1]);

	assert(i->blocks);
	
	dirent* d = (dirent*) b->data;
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
			printf("%d\t/%s\trec: %d\n", d->inode, d->name, d->rec_len);
			d->rec_len = calc; 		// Resize this entry to it's real size
			d = (dirent*)((uint32_t) d + d->rec_len);

			break;


		}
		if (d->rec_len)
			printf("%d\t/%s\trec: %d\n", d->inode, d->name, d->rec_len);
		//printf("name %s\n",d->name);
		d = (dirent*)((uint32_t) d + d->rec_len);
		//if (d->rec_len == 0)
		//	break;
	} while(sum < 1024);

	uint32_t this_inode = ext2_alloc_inode();
	superblock* s = ext2_superblock(1);
	int block_group = (this_inode - 1) / s->inodes_per_group; // block group #

	/* d is now pointing at a blank entry, right after the resized last entry */
	d->rec_len = BLOCK_SIZE - ((uint32_t)d - (uint32_t)b->data);
	d->inode = this_inode;
	d->file_type = 2;
	d->name_len = strlen(name);

	/* memcpy() causes a page fault */
	for (int q = 0; q < strlen(name); q++) 
		d->name[q] = name[q];

	buffer_write(b);	// Update the parent directory blocks

	inode* thisi = malloc(sizeof(inode)); 
	thisi->blocks = 2;
	puts("DEBUG1");
	thisi->block[0] = ext2_alloc_block(block_group);
	puts("DEBUG2");
	thisi->mode = mode;		// File
	thisi->size = n;
	thisi->atime = time();
	thisi->ctime = time();
	thisi->mtime = time();
	thisi->dtime = 0;
	thisi->links_count = 1;		/* Setting this to 0 = BIG NO NO */

	ext2_write_inode(1, this_inode, i);

	return NULL;

	//ext2_write_file(this_inode, name, data, mode, n);
}

/* Add an inode into parent_inode, increase link count */
int ext2_link_dirent(int parent_inode, int this_inode) {

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