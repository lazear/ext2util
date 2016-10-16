// sync.c

#include "ext2.h"
#include "fs.h"
#include <stdio.h>

struct super_block* ext2_read_super(struct super_block*, void*, int);
extern void ext2_read_inode(struct inode*);
extern void ext2_write_inode(struct inode*);

struct file_system_type ext2_fs_type = {
	.read_super = ext2_read_super,
	.name = "ext2",
	.requires_dev = 1,
	.next = NULL
};


struct super_operations ext2_super_operations = {
	.alloc_inode = NULL,
	.read_inode = ext2_read_inode,
	.write_inode = ext2_write_inode,
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

void spinlock_lock(uint32_t* lock) {
	__sync_val_compare_and_swap(lock, 0, 1);
	printf("lock vaL %d\n", *(uint32_t*)lock);
}

extern void acquire(uint32_t*);
extern void release(uint32_t*);


struct ext2_group_desc* ext2_get_group_desc (struct super_block* sb, int n) {
	if (n > sb->u.ext2_sb.s_groups_count)
		return NULL;
	int group  = (n / (sb->s_blocksize / sizeof(struct ext2_group_desc)));
	int offset = (n % (sb->s_blocksize / sizeof(struct ext2_group_desc)));
	struct ext2_group_desc* gd = sb->u.ext2_sb.s_gd_buffer[group]->data;
	return gd + offset;
}


struct super_block* ext2_read_super(struct super_block* sb, void* data, int silent) {

	/* 
	1. Read superblock from disk
	2. Read root inode from disk
	*/

	printf("ext2_read_super()\n");
	struct ext2_superblock * es;

	struct ide_buffer* b;
	kdev_t dev = sb->s_dev;
	int block = 1;

	b = buffer_read2(dev, EXT2_SUPER, 1024);
	es = (struct ext2_superblock*) b->data;
	sb->u.ext2_sb.s_es = es;
	sb->u.ext2_sb.s_sb_buffer = b;

	if (es->magic != EXT2_MAGIC) {
		if (!silent)
			printf("EXT2: invalid superblock on disk\n");
		buffer_free(b);
		return NULL;
	}

	sb->u.ext2_sb.s_inodes_count = es->inodes_count;
	sb->u.ext2_sb.s_blocks_count = es->blocks_count;
	sb->u.ext2_sb.s_blocks_per_group = es->blocks_per_group;
	sb->u.ext2_sb.s_groups_count = (es->blocks_count / es->blocks_per_group);
	sb->s_type = &ext2_fs_type;
	sb->s_blocksize = (1024 << es->log_block_size);
	sb->s_access = 0x3;
	sb->s_ops = &ext2_super_operations;

	/* Create a list of buffers containing the block group descriptors */
	int num_block_groups = sb->u.ext2_sb.s_groups_count;
	int num_to_read = ((num_block_groups * sizeof(struct ext2_group_desc)) / sb->s_blocksize) + 1;
	sb->u.ext2_sb.s_gd_buffer = malloc(num_to_read * sizeof(struct ide_buffer*));
	for (int i = 0; i < num_to_read; i++) {
		int n = EXT2_SUPER + i + ((sb->s_blocksize == 1024) ? 1 : 0); 
		sb->u.ext2_sb.s_gd_buffer[i] = buffer_read2(sb->s_dev, n, sb->s_blocksize);
	}

	/* Load the root inode of the file system */
	sb->s_mounted = get_inode(sb, EXT2_ROOTDIR);

	return sb;
}


int ext2_initialize_fs(){
	return register_filesystem(&ext2_fs_type);
}

struct super_block* ext2_mount(int dev) {

	struct super_block* sb = malloc(sizeof(struct super_block));
	ext2_read_super(sb, NULL, 0);

	printf("DUMPING EXT2 SUPERBLOCK\n");
	sb_dump(&sb->u.ext2_sb);
	bg_dump2(sb);
	inode_dump2(sb->s_mounted);
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
