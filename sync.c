// sync.c

#include "ext2.h"
#include "fs.h"
#include <stdio.h>

struct super_block* ext2_read_super(struct super_block*, void*, int);

extern void ext2_read_inode_new(struct inode* inode);
extern void ext2_write_inode_new(struct inode* inode);


struct file_system_type ext2_fs_type = {
	.read_super = ext2_read_super,
	.name = "ext2",
	.requires_dev = 1,
	.next = NULL
};


struct super_operations ext2_super_operations = {
	.alloc_inode = NULL,
	.read_inode = ext2_read_inode_new,
	.write_inode = ext2_write_inode_new,
	.sync_fs = NULL,
	.write_super = NULL,
};

struct inode_operations ext2_file_inode_operations = {
	.create = NULL,
	.link = NULL,
	.unlink = NULL,
	.mkdir = NULL,
	.rmdir = NULL,
	.rename = NULL,
};

struct file_operations ext2_default_file_operations = {
	.open = NULL,
	.close = NULL,
	.read = NULL,
	.write = NULL,
};

void acquire_fs(struct ext2_fs *f) {}

void release_fs(struct ext2_fs *f) {}


void sync(struct ext2_fs *f) {
	f->sb->wtime = time(NULL);
	ext2_superblock_write(f);
	ext2_blockdesc_write(f);
}
void ext2_global_test(struct inode* inode) {

}

/* ext2_read_super
	struct super_block* sb -- vfs super block structure
	void* data -- mount options? ignore for now
	int silent -- do we output or not?
*/

struct super_block* ext2_read_super(struct super_block* sb, void* data, int silent) {

	/* 
	1. Read superblock from disk
	2. Read root inode from disk
	*/

	struct ext2_superblock * es;
	struct ide_buffer* b;
	kdev_t dev = sb->s_dev;
	int block = 1;

	b = buffer_read2(dev, EXT2_SUPER, 1024);
	es = (struct ext2_superblock*) b->data;

	if (es->magic != EXT2_MAGIC) {
		if (!silent)
			printf("EXT2: invalid superblock on disk\n");
		buffer_free(b);
		return NULL;
	}

	sb->s_type = &ext2_fs_type;
	sb->s_blocksize = (1024 << es->log_block_size);
	sb->s_access = 0x3;
	sb->s_ops = &ext2_super_operations;
	memcpy((void*) &sb->u.ext2_sb, es, sizeof(struct ext2_superblock));
	sb->s_mounted = get_inode(sb, EXT2_ROOTDIR);
	
	/* load root inode */

	return sb;
}


int ext2_initialize_fs(){
	return register_filesystem(&ext2_fs_type);
}

struct super_block* ext2_mount(int dev) {
	struct super_block* sb = malloc(sizeof(struct super_block));
	ext2_initialize_fs();
	ext2_read_super(sb, NULL, 0);

	sb_dump(&sb->u.ext2_sb);

	return sb;
}


/* Returns inode of file in a path /usr/sbin/file.c would return the inode
number of file.c, or -1 if any member of the path cannot be found */
int pathize(struct ext2_fs* f, char* path) {

	
	char* pch = strtok(path, "/");

	int parent = 2;
	while(pch) {
		parent = ext2_find_child(f, pch, parent);
		//printf("%s inode: %i\n", pch, parent);
		pch = strtok(NULL, "/");
	}
	return parent;
}
