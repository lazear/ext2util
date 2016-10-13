// sync.c

#include "ext2.h"
#include <stdio.h>


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
	efs->dev = dev;
	efs->block_size = 1024;
	efs->sb = NULL;
	efs->bg = NULL;

	struct ide_buffer* list = new_buffer(efs);
	list->head = &list;
	list->last = list;

	efs->cache = &list;

	printf("%x\n", *efs->cache);
	printf("block size: %d\n", efs->block_size);
	ext2_superblock_read(efs);

	ext2_blockdesc_read(efs);

	bg_dump(efs);

	return efs;
}