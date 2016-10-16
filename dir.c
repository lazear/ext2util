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
int ext2_add_child(struct inode* inode, struct dentry* parent, char* name, uint16_t mode) {
	struct super_block* sb = inode->i_sb;
	int num_to_read = (parent->d_inode->u.ext2_i.blocks / (inode->i_sb->s_blocksize / SECTOR_SIZE)); 
	struct ide_buffer* b = buffer_read2(sb->s_dev, inode->u.ext2_i.block[num_to_read], sb->s_blocksize);
	struct ext2_dirent* d = (struct ext2_dirent*) b->data;

	int sum = 0;
	int calc = 0;
	int new_entry_len = (sizeof(struct ext2_dirent) + strlen(name) + 4) & ~0x3;
	do {
		// Calculate the 4byte aligned size of each entry
		calc = (sizeof(struct ext2_dirent) + d->name_len + 4) & ~0x3;
		sum += d->rec_len;

		/* Child already exists here */
		if (d->inode == inode->i_ino)
			return -1;

		// if (strcmp(d->name, name) == 0)
		// 	return -1;

		if (d->rec_len != calc && sum == sb->s_blocksize) {
			/* if the calculated value doesn't match the given value,
			then we've reached the final entry on the block */
			sum -= d->rec_len;
			if (sum + new_entry_len > sb->s_blocksize) {
				/* we need to allocate another block for the parent
				directory*/
				printf("PANIC! out of room");
				return NULL;
			}
			d->rec_len = calc; 		// Resize inode entry to it's real size
			d = (struct ext2_dirent*)((uint32_t) d + d->rec_len);
			break;
		}
		//printf("name %s\n",d->name);
		d = (struct ext2_dirent*)((uint32_t) d + d->rec_len);

		if (d->rec_len == 0)
			break;
	} while(sum < sb->s_blocksize);

	/* d is now pointing at a blank entry, right after the resized last entry */
	d->rec_len 		= sb->s_blocksize - ((uint32_t)d - (uint32_t)b->data);
	d->inode 		= inode->i_ino;
	d->file_type 	= mode;
	d->name_len 	= strlen(name);

	for (int q = 0; q < strlen(name); q++) 
		d->name[q] = name[q];

	/* Write the buffer to the disk */
	buffer_write2(b);

	return 1;
}

/* Finds an inode by name in dir_inode */
int ext2_find_child(struct dentry* parent, const char* name) {
	if (!parent)
		return -1;
	struct ext2_inode* i = &parent->d_inode->u.ext2_i;
	struct super_block* sb = parent->d_inode->i_sb;
	int num_blocks = i->blocks / (sb->s_blocksize / SECTOR_SIZE);
	char* buf = malloc(sb->s_blocksize*num_blocks);
	memset(buf, 0, sb->s_blocksize*num_blocks);

	for (int q = 0; q < i->blocks / (sb->s_blocksize/SECTOR_SIZE); q++) {
		buffer* b = buffer_read2(sb->s_dev, i->block[q], sb->s_blocksize);
		memcpy((uint32_t)buf+(q * sb->s_blocksize), b->data, sb->s_blocksize);
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
struct ext2_dirent* ext2_create_dir(struct inode* inode, struct dentry* parent, uint16_t mode) {
	 //int mode = EXT2_IFDIR | EXT2_IRUSR | EXT2_IWUSR | EXT2_IXUSR;
	//int inode->i_ino = ext2_alloc_inode(f);
	//int block_group = (inode->i_ino - 1) / s->inodes_per_group; // block group #
	//struct inode* new =  ext2_alloc_inode(inode->i_sb);
	struct ext2_inode* in = &inode->u.ext2_i;
	in->blocks = 2;
	puts("DEBUG1");
	//in->block[0] = ext2_alloc_block(f, 1);
	puts("DEBUG2");
	in->mode = mode;	
	in->size = 0;
	in->atime = time(NULL);
	in->ctime = time(NULL);
	in->mtime = time(NULL);
	in->dtime = 0;
	in->links_count = 1;		/* Setting this to 0 = BIG NO NO */


	ext2_write_inode(inode);
	//assert(ext2_add_child(inode, parent, name, mode));

	return inode->i_ino;
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

void ls(struct inode* dentry) {
	//struct ext2_inode* i = ext2_read_inode(f, inode_num);			// Root directory
	struct ext2_inode* i = &dentry->u.ext2_i;
	struct super_block* sb = dentry->i_sb;
	int num_blocks = i->blocks / (sb->s_blocksize / SECTOR_SIZE);
	int sz = sb->s_blocksize*num_blocks;
	char* buf = malloc(sz);
	memset(buf, 0, sz);

	for (int q = 0; q < num_blocks; q++) {
		printf("%d\n", i->block[q]);
		buffer* b = buffer_read2(sb->s_dev, i->block[q], sb->s_blocksize);
		memcpy((uint32_t)buf + (q*sb->s_blocksize), b->data, sb->s_blocksize);
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
		
		//struct inode* di = ext2_read_inode(f, d->inode);
		//char* perm = gen_file_perm_string(di->mode);
		printf("%5d %20s\n", d->inode, d->name);//, di->size);
		//free(perm);

		d = (struct ext2_dirent*)((char*)d + d->rec_len);
	} while(sum < (sb->s_blocksize * num_blocks));

	free(buf);
	return NULL;
}


