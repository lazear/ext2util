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
int ext2_add_child(struct ext2_fs *f, int parent_inode, int i_no, char* name, int type) {

	struct ext2_inode* rootdir = ext2_read_inode(f, parent_inode);
	assert(rootdir->blocks);
	assert(rootdir->block[0]);

	buffer* rootdir_buf = buffer_read(f, rootdir->block[0]);
	struct ext2_dirent* d = (struct ext2_dirent*) rootdir_buf->data;

	int sum = 0;
	int calc = 0;
	int new_entry_len = (sizeof(struct ext2_dirent) + strlen(name) + 4) & ~0x3;
	do {
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(struct ext2_dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;

		/* Child already exists here */
		if (d->inode == i_no)
			return -1;

		// if (strcmp(d->name, name) == 0)
		// 	return -1;

		if (d->rec_len != calc && sum == f->block_size) {
			/* if the calculated value doesn't match the given value,
			then we've reached the final entry on the block */
			sum -= d->rec_len;
			if (sum + new_entry_len > f->block_size) {
				/* we need to allocate another block for the parent
				directory*/
				printf("PANIC! out of room");
				return NULL;
			}
			d->rec_len = calc; 		// Resize this entry to it's real size
			d = (struct ext2_dirent*)((uint32_t) d + d->rec_len);
			break;
		}
		//printf("name %s\n",d->name);
		d = (struct ext2_dirent*)((uint32_t) d + d->rec_len);

		if (d->rec_len == 0)
			break;
	} while(sum < f->block_size);

	/* d is now pointing at a blank entry, right after the resized last entry */
	d->rec_len 		= f->block_size - ((uint32_t)d - (uint32_t)rootdir_buf->data);
	d->inode 		= i_no;
	d->file_type 	= 1;
	d->name_len 	= strlen(name);

	for (int q = 0; q < strlen(name); q++) 
		d->name[q] = name[q];

	/* Write the buffer to the disk */
	buffer_write(f, rootdir_buf);

	return 1;
}

/* Finds an inode by name in dir_inode */
int ext2_find_child(struct ext2_fs *f, const char* name, int dir_inode) {
	if (!dir_inode)
		return -1;
	struct ext2_inode* i = ext2_read_inode(f, dir_inode);			// Root directory
	int num_blocks = i->blocks / (f->block_size / SECTOR_SIZE);
	char* buf = malloc(f->block_size*num_blocks);
	memset(buf, 0, f->block_size*num_blocks);

	for (int q = 0; q < i->blocks / 2; q++) {
		buffer* b = buffer_read(f, i->block[q]);
		memcpy((uint32_t)buf+(q * f->block_size), b->data, f->block_size);
	}

	struct ext2_dirent* d = (struct ext2_dirent*) buf;
	
	int sum = 0;
	int calc = 0;
	do {
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(struct ext2_dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;
		//printf("%2d  %10s\t%2d %3d\n", (int)d->inode, d->name, d->name_len, d->rec_len);
		if (strncmp(d->name, name, d->name_len)== 0) {
			
			free(buf);
			return d->inode;
		}
		d = (struct ext2_dirent*)((uint32_t) d + d->rec_len);

	} while(sum < (1024 * num_blocks));
	free(buf);
	return -1;
}


/* Create a new directory
Need to handle cases where this entry causes an overflow of the parent directory,
i.e. force allocation of a new block */
struct ext2_dirent* ext2_create_dir(struct ext2_fs *f, char* name, int parent_inode) {
	int mode = EXT2_IFDIR | EXT2_IRUSR | EXT2_IWUSR | EXT2_IXUSR;
	int i_no = ext2_alloc_inode(f);
	//int block_group = (i_no - 1) / s->inodes_per_group; // block group #

	struct ext2_inode* in = malloc(INODE_SIZE); 
	in->blocks = 2;
	puts("DEBUG1");
	in->block[0] = ext2_alloc_block(f, 1);
	puts("DEBUG2");
	in->mode = mode;	
	in->size = 0;
	in->atime = time(NULL);
	in->ctime = time(NULL);
	in->mtime = time(NULL);
	in->dtime = 0;
	in->links_count = 1;		/* Setting this to 0 = BIG NO NO */


	ext2_write_inode(f, i_no, in);
	assert(ext2_add_child(f, parent_inode, i_no, name, EXT2_FT_DIR));

	return i_no;
}

char* gen_file_perm_string(uint16_t x) {
	char *perm = malloc(10);
	strcpy(perm, "---------");

	if (x & EXT2_IFDIR) perm[0] = 'd';
	if (x & EXT2_IRUSR) perm[1] = 'r';		//user read
	if (x & EXT2_IWUSR) perm[2] = 'w';		//user write
	if (x & EXT2_IXUSR) perm[3] = 'x';		//user execute
	if (x & EXT2_IRGRP) perm[4] = 'r';		//group read
	if (x & EXT2_IWGRP) perm[5] = 'w';		//group write
	if (x & EXT2_IXGRP) perm[6] = 'x';		//group execute
	if (x & EXT2_IROTH) perm[7] = 'r';		//others read
	if (x & EXT2_IWOTH) perm[8] = 'w';		//others write
	if (x & EXT2_IXOTH) perm[9] = 'x';
	return perm;
}

void ls(struct ext2_fs *f, int inode_num) {
	struct ext2_inode* i = ext2_read_inode(f, inode_num);			// Root directory
	int num_blocks = i->blocks / (f->block_size / SECTOR_SIZE);
	int sz = f->block_size*num_blocks;
	char* buf = malloc(sz);
	memset(buf, 0, sz);

	for (int q = 0; q < num_blocks; q++) {
		printf("%d\n", i->block[q]);
		buffer* b = buffer_read(f, i->block[q]);
		memcpy((uint32_t)buf + (q*f->block_size), b->data, f->block_size);
		buffer_free(b);
	}

	struct ext2_dirent* d = (struct ext2_dirent*) buf;
	
	int sum = 0;
	int calc = 0;
	printf("inode permission %20s\tsize\n", "name");	
	do {
		
		// Calculate the 4byte aligned size of each entry
		sum += d->rec_len;
		if (d->rec_len == 0)
			break;
		
		struct ext2_inode* di = ext2_read_inode(f, d->inode);
		char* perm = gen_file_perm_string(di->mode);
		printf("%5d %s %20s\t%d\n", d->inode, perm, d->name, di->size);
		free(perm);

		d = (struct ext2_dirent*)((char*)d + d->rec_len);
	} while(sum < (f->block_size * num_blocks));

	free(buf);
	return NULL;
}


