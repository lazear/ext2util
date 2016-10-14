// sync.c

#include "ext2.h"
#include "fs.h"
#include <stdio.h>

/* Core virtual filesystem abstraction layer */


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
	void* data -- pre-allocated destination
	int silent -- do we output or not?
*/

struct super_block* ext2_read_super(struct super_block* sb, void* data, 
	int silent) {

	struct ext2_super_block * es;
	struct ide_buffer* b;
	kdev_t dev = sb->s_dev;
	int block = 1;


}

struct file_system_type ext2_fs_type = {
	.read_super = NULL,
	.name = "ext2",
	.requires_dev = 1,
	.next = NULL
};

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
