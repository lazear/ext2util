/* Virtual filesystem abstraction. Heavily inspired by the Linux kernel */
#include <stdint.h>
#include <stdio.h>
#include "ext2.h"
#include "errno.h"

#define GLOBAL


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
GLOBAL struct file_system_type* file_systems;

/* Abstraction for superblocks, derived from Linux kernel code */
struct super_block {
	kdev_t s_dev;
	uint32_t s_blocksize;
	uint8_t s_access;
	struct file_system_type* s_type;
	struct inode* s_mounted;
	struct super_operations* s_ops;
	union {
		/* Store the on-disk data structures */
		struct ext2_superblock* ext2_sb;
		/* Add other filesystem implementations... */
	}u;
};

struct inode {
	uint16_t i_dev;
	uint32_t i_ino;
	uint16_t i_nlinks;	// How many links
	uint16_t i_mode;	// Format of the file, and access rights
	uint16_t i_id;		// User id associated with file
	uint16_t i_gid;		// POSIX group access
	uint32_t i_size;	// Size of file in bytes
	uint32_t i_atime;	// Last access time, POSIX
	uint32_t i_ctime;	// Creation time
	uint32_t i_mtime;	// Last modified time
	
	struct inode_operations* i_ops;
	union {
		/* Store the on-disk data structures */
		struct ext2_inode* ext2_i;
		/* Add other filesystem implementations... */
	} u;

};

struct file {
	uint16_t f_mode;	// Read/Writable
	uint32_t f_flags;

	uint64_t f_pos;		// Current read/write position
	void* private_data;

	struct file_operations* f_ops;

};

struct file_operations {
	int (*open);
	int (*close);
	size_t (*read) (struct file*, char* __user, size_t cnt);
	size_t (*write) (struct file*, char* __user, size_t cnt);
};

struct inode_operations {
	int (*create) (struct inode*);
};

struct super_operations {
	void (*read_superblock);
};

