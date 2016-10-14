/* Virtual filesystem abstraction. Heavily inspired by the Linux kernel */
#ifndef __vfs__
#define __vfs__

#include <stdint.h>
#include <stdio.h>
#include "ext2.h"
#include "errno.h"

typedef unsigned short kdev_t;

#define MAJOR(k)	((k) >> 8);
#define MINOR(k)	((k) & ((1<<8)-1));

struct file_system_type {
	struct super_block *(*read_super) (struct super_block*, void*, int);
	const char* name;
	int requires_dev;
	struct file_system_type* next;	/* Singly linked list for global */
};

/* Global variable */
struct file_system_type* file_systems;

/* Abstraction for superblocks, derived from Linux kernel code */
struct super_block {
	kdev_t s_dev;
	uint32_t s_blocksize;
	uint8_t s_access;
	struct file_system_type* s_type;
	struct inode* s_mounted;
	const struct super_operations* s_ops;

	union {
		/* Store the on-disk data structures */
		struct ext2_superblock ext2_sb;
		/* Add other filesystem implementations... */
	}u;
};

struct inode {
	kdev_t i_dev;
	uint32_t i_ino;
	uint16_t i_nlinks;	// How many links
	uint16_t i_mode;	// Format of the file, and access rights
	uint16_t i_id;		// User id associated with file
	uint16_t i_gid;		// POSIX group access
	uint32_t i_size;	// Size of file in bytes
	uint32_t i_atime;	// Last access time, POSIX
	uint32_t i_ctime;	// Creation time
	uint32_t i_mtime;	// Last modified time
	
	const struct inode_operations* i_ops;

	struct super_block* i_sb;
	union {
		/* Store the on-disk data structures */
		struct ext2_inode ext2_i;
		/* Add other filesystem implementations... */
	} u;

};

#define DENTRY_NAME_LEN 	32
struct dentry {
	uint32_t d_flags;
	struct dentry* parent;		/* parent directory */
	struct inode* d_inode;		/* this inode */
	uint8_t d_iname[DENTRY_NAME_LEN];

	struct super_block* d_sb;

	struct dentry* d_peers;		/* peers of this directory */
	struct dentry* d_children;	/* children of this directory */
};

struct file {
	uint16_t f_mode;	// Read/Writable
	uint32_t f_flags;

	uint64_t f_pos;		// Current read/write position
	void* private_data;

	const struct file_operations* f_ops;

};

struct file_operations {
	int (*open) (struct inode*, struct file*);
	int (*close) (struct inode*, struct file*);
	size_t (*read) (struct file*, char* __user, size_t cnt);
	size_t (*write) (struct file*, char* __user, size_t cnt);
};

struct inode_operations {
	int (*create) (struct inode*, struct dentry*, uint16_t mode);
	int (*link)	(struct inode*, struct inode*, struct dentry*);
	int (*unlink) (struct inode*, struct dentry*);
	int (*mkdir) (struct inode*, struct dentry*, uint16_t mode);
	int (*rmdir) (struct inode*, struct dentry*);
	int (*rename) (struct inode*, struct dentry*, struct inode*, struct dentry*);
};

struct super_operations {
	struct inode* (*alloc_inode)(struct super_block*);
	int (*read_inode) (struct inode* inode);
	int (*write_inode) (struct inode* inode);
	int (*sync_fs) (struct super_block*);
	int (*write_super) (struct super_block*);
};

/*
ext2/super.c

323 static const struct super_operations ext2_sops = {
324         .alloc_inode    = ext2_alloc_inode,
325         .destroy_inode  = ext2_destroy_inode,
326         .write_inode    = ext2_write_inode,
327         .evict_inode    = ext2_evict_inode,
328         .put_super      = ext2_put_super,
329         .sync_fs        = ext2_sync_fs,
330         .freeze_fs      = ext2_freeze,
331         .unfreeze_fs    = ext2_unfreeze,
332         .statfs         = ext2_statfs,
333         .remount_fs     = ext2_remount,
334         .show_options   = ext2_show_options,
335 #ifdef CONFIG_QUOTA
336         .quota_read     = ext2_quota_read,
337         .quota_write    = ext2_quota_write,
338         .get_dquots     = ext2_get_dquots,
339 #endif
340 };

ext2/file.c

163 const struct file_operations ext2_file_operations = {
164         .llseek         = generic_file_llseek,
165         .read_iter      = generic_file_read_iter,
166         .write_iter     = generic_file_write_iter,
167         .unlocked_ioctl = ext2_ioctl,
168 #ifdef CONFIG_COMPAT
169         .compat_ioctl   = ext2_compat_ioctl,
170 #endif
171         .mmap           = ext2_file_mmap,
172         .open           = dquot_file_open,
173         .release        = ext2_release_file,
174         .fsync          = ext2_fsync,
175         .splice_read    = generic_file_splice_read,
176         .splice_write   = iter_file_splice_write,
177 };
178 
179 const struct inode_operations ext2_file_inode_operations = {
180 #ifdef CONFIG_EXT2_FS_XATTR
181         .setxattr       = generic_setxattr,
182         .getxattr       = generic_getxattr,
183         .listxattr      = ext2_listxattr,
184         .removexattr    = generic_removexattr,
185 #endif
186         .setattr        = ext2_setattr,
187         .get_acl        = ext2_get_acl,
188         .set_acl        = ext2_set_acl,
189         .fiemap         = ext2_fiemap,
190 };
191 
*/


#endif