// sync.c

#include "ext2.h"
#include <stdio.h>

/* Core virtual filesystem abstraction layer */


struct filesystem* device_list = NULL;

void acquire_fs(struct ext2_fs *f) {

}

void release_fs(struct ext2_fs *f) {

}

void sync(struct ext2_fs *f) {
	f->sb->wtime = time(NULL);
	ext2_superblock_write(f);
	ext2_blockdesc_write(f);
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

	fs_dev_register(dev, &vfs);
	ext2_superblock_read(efs);

	ext2_blockdesc_read(efs);

	bg_dump(efs);

	return efs;
}

int fs_dev_register(int dev, struct filesystem* f) {
	if (dev >= MAX_DEVICES || !device_list)
		return -1;
	device_list[dev] = *f;
}
void fs_dev_init() {
	if (device_list)
		return;
	device_list = malloc(sizeof(struct filesystem) * MAX_DEVICES);
}

/* Virtual filesystem layer. Search through the mount point list.
If the mount name is not found, default to root */
struct filesystem* fs_dev_from_mount(char* mount) {
	if (!device_list)
		return NULL;
	for (int i = 0; i < MAX_DEVICES; i++)
		if (strcmp(device_list[i].mount, mount) == 0)
			return &device_list[i];
	return fs_dev_from_mount(ROOT_NAME);
}

void trav_device_list() {
	if (!device_list)
		return NULL;
	for (int i = 0; i < MAX_DEVICES; i++)
		printf("device %d: %s\ttype: %d\tfs: %x\n", i, device_list[i].mount, device_list[i].type, device_list[i].data);
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
