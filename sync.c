// sync.c

#include "ext2.h"
#include "fs.h"
#include <stdio.h>

struct super_block* ext2_read_super(struct super_block*, void*, int);

struct file_system_type ext2_fs_type = {
	.read_super = ext2_read_super,
	.name = "ext2",
	.requires_dev = 1,
	.next = NULL
};


struct super_operations ext2_super_operations = {
	.alloc_inode = NULL,
	.read_inode = NULL,
	.write_inode = NULL,
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



void acquire_fs(struct ext2_fs *f) {

}

void release_fs(struct ext2_fs *f) {

}

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
	sb->u.ext2_sb = es;

	/* load root inode */

	return sb;
}


int ext2_initialize_fs(){
	return register_filesystem(&ext2_fs_type);
}

struct ext2_fs* ext2_mount(int dev) {
	struct ext2_fs* efs = malloc(sizeof(struct ext2_fs));
	struct filesystem vfs;

	efs->dev = dev;
	efs->block_size = 1024;
	efs->sb = NULL;
	efs->bg = NULL;

	strcpy(vfs.mount, ROOT_NAME);
	vfs.type = EXT2;
	vfs.data.fs = efs;
	vfs.dev = dev;

	ext2_superblock_read(efs);

	ext2_blockdesc_read(efs);

	bg_dump(efs);

	return efs;
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
