/* dentry.c */

#include "fs.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


// #define DENTRY_NAME_LEN 	32
// struct dentry {
// 	uint32_t d_flags;
// 	struct dentry* parent;		/* parent directory */
// 	struct inode* d_inode;		/* this inode */
// 	uint8_t d_iname[DENTRY_NAME_LEN];

// 	struct super_block* d_sb;

// 	struct dentry* d_peers;		/* peers of this directory */
// 	struct dentry* d_children;	/* children of this directory */
// };

struct list_head dcache;

void dentry_add_to_cache(struct dentry* d) {
	list_add(&d->d_cache, &dcache);
}

struct dentry* dentry_new(struct inode* inode) {
	struct dentry* d = malloc(sizeof(struct dentry));
	d->d_inode = inode;
	d->d_sb = inode->i_sb;

	dentry_add_to_cache(d);
}

void dentry_traverse() {
	struct list_head* lru;
	int i = 0;
	for (lru = &dcache; lru; lru = lru->next) {
		struct dentry* d = list_entry(lru, struct dentry, d_cache);
		printf("dentry %d inode %d \n", i++, d->d_inode->i_ino);
	}
}

void dentry_test() {

	return;
	//dcache = malloc(sizeof(struct list_head));	
	for (int i = 0; i < 10; i++) {
		struct inode* inode = malloc(sizeof(struct inode));
		inode->i_ino = i;
		printf("making inode %d\n", i);
		dentry_new(inode);
	}
	dentry_traverse();


}