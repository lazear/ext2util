/*
ext2.h
================================================================================
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
================================================================================
*/


/*
http://www.nongnu.org/ext2-doc/ext2.html

Block groups are found at the address (group number - 1) * blocks_per_group.
Each block group has a backup superblock as it's first block
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifndef __vfs__
#include "fs.h"
#endif

#ifndef __baremetal_ext2__
#define __baremetal_ext2__


#define SECTOR_SIZE		512
#define EXT2_BOOT		0			// Block 0 is bootblock
#define EXT2_SUPER		1			// Block 1 is superblock
#define EXT2_ROOTDIR	2
#define EXT2_MAGIC		0x0000EF53
#define EXT2_IND_BLOCK 	12


#define EXT2_SB(s) 	((s)->u.ext2_sb.s_es)
#define EXT2_ADDRESS_PER_BLOCK(s) 	((s)->s_block_size / sizeof(uint32_t))

struct ext2_superblock_info {
	uint32_t s_inodes_count;			// Total # of inodes
	uint32_t s_blocks_count;			// Total # of blocks
	uint32_t s_blocks_per_group;
	uint32_t s_inodes_per_group;
	uint32_t s_desc_per_block;
	uint32_t s_groups_count;

	struct ext2_superblock *s_es;
	struct ide_buffer* s_sb_buffer;		/* Save locked buffers */
	struct ide_buffer** s_gd_buffer;
};

struct ext2_superblock {
	uint32_t inodes_count;			// Total # of inodes
	uint32_t blocks_count;			// Total # of blocks
	uint32_t r_blocks_count;		// # of reserved blocks for superuser
	uint32_t free_blocks_count;	
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;		// 1024 << Log2 block size  = block size
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;					// Last mount time, in POSIX time
	uint32_t wtime;					// Last write time, in POSIX time
	uint16_t mnt_count;				// # of mounts since last check
	uint16_t max_mnt_count;			// # of mounts before fsck must be done
	uint16_t magic;					// 0xEF53
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev_level;
	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;
	uint16_t def_resuid;
	uint16_t def_resgid;
} __attribute__((packed));

/*
Inode bitmap size = (inodes_per_group / 8) / BLOCK_SIZE
block_group = (block_number - 1)/ (blocks_per_group) + 1
*/
struct ext2_group_desc {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad[7];
} __attribute__((packed));


/*
maximum value of inode.block[index] is inode.blocks / (2 << log_block_size)

Locating an inode:
block group = (inode - 1) / s_inodes_per_group

inside block:
local inode index = (inode - 1) % s_inodes_per_group

containing block = (index * INODE_SIZE) / BLOCK_SIZE
*/
struct ext2_inode {
	uint16_t mode;			// Format of the file, and access rights
	uint16_t uid;			// User id associated with file
	uint32_t size;			// Size of file in bytes
	uint32_t atime;			// Last access time, POSIX
	uint32_t ctime;			// Creation time
	uint32_t mtime;			// Last modified time
	uint32_t dtime;			// Deletion time
	uint16_t gid;			// POSIX group access
	uint16_t links_count;	// How many links
	uint32_t blocks;		// # of 512-bytes blocks reserved to contain the data
	uint32_t flags;			// EXT2 behavior
	uint32_t osdl;			// OS dependent value
	uint32_t block[15];		// Block pointers. Last 3 are indirect
	uint32_t generation;	// File version
	uint32_t file_acl;		// Block # containing extended attributes
	uint32_t dir_acl;
	uint32_t faddr;			// Location of file fragment
	uint32_t osd2[3];
} __attribute__((packed));

#define INODE_SIZE (sizeof(struct ext2_inode))

#define EXT2_IFSOCK		0xC000		//socket
#define EXT2_IFLNK		0xA000		//symbolic link
#define EXT2_IFREG		0x8000		//regular file
#define EXT2_IFBLK		0x6000		//block device
#define EXT2_IFDIR		0x4000		//directory
#define EXT2_IFCHR		0x2000		//character device
#define EXT2_IFIFO		0x1000		//fifo
//-- process execution user/group override --
#define EXT2_ISUID		0x0800		//Set process User ID
#define EXT2_ISGID		0x0400		//Set process Group ID
#define EXT2_ISVTX		0x0200		//sticky bit
//-- access rights --
#define EXT2_IRUSR		0x0100		//user read
#define EXT2_IWUSR		0x0080		//user write
#define EXT2_IXUSR		0x0040		//user execute
#define EXT2_IRGRP		0x0020		//group read
#define EXT2_IWGRP		0x0010		//group write
#define EXT2_IXGRP		0x0008		//group execute
#define EXT2_IROTH		0x0004		//others read
#define EXT2_IWOTH		0x0002		//others write
#define EXT2_IXOTH		0x0001		//others execute


#define EXT2_FT_UNKNOWN		0	// Unknown File Type
#define EXT2_FT_REG_FILE	1	// Regular File
#define EXT2_FT_DIR			2	// Directory File
#define EXT2_FT_CHRDEV		3	// Character Device
#define EXT2_FT_BLKDEV		4	// Block Device
#define EXT2_FT_FIFO		5	// Buffer File
#define EXT2_FT_SOCK		6	// Socket File
#define EXT2_FT_SYMLINK		7	// Symbolic Lin


/*
Directories must be 4byte aligned, and cannot extend between multiple
blocks on the disk */
struct ext2_dirent {
	uint32_t inode;			// Inode
	uint16_t rec_len;		// Total size of entry, including all fields
	uint8_t name_len;		// Name length, least significant 8 bits
	uint8_t file_type;		// Type indicator
	uint8_t name[];
} __attribute__((packed));

/* IMPORTANT: Inode addresses start at 1 */

typedef struct ide_buffer {
	uint32_t block;				// block number
	int flags;
	uint8_t* data;	// 1 disk sector of data
	uint32_t size;
	uint16_t dev;
} buffer;


struct ext2_fs {
	int dev;
	int block_size;
	int num_bg;
	int mutex;
	struct ext2_superblock* sb;
	struct ext2_group_desc* bg;
};

struct ext2_fs* gfsp;

#define B_BUSY	0x1		// buffer is locked by a process
#define B_VALID	0x2		// buffer has been read from disk
#define B_DIRTY	0x4		// buffer has been written to

/* Function definitions */

/* Replace when on real hardware */
// extern buffer* buffer_read(struct ext2_fs *f, int block);
// extern uint32_t buffer_write(struct ext2_fs *f, buffer* b);
// extern buffer* buffer_read_superblock(struct ext2_fs* f);
// extern uint32_t buffer_write_superblock(struct ext2_fs *f, buffer* b);
// extern int buffer_free(buffer* b);

// /* ext2.c */
// extern int ext2_superblock_read(struct ext2_fs *f);
// extern int ext2_superblock_write(struct ext2_fs *f);
// extern int ext2_blockdesc_read(struct ext2_fs *f);
// extern int ext2_blockdesc_write(struct ext2_fs *f);
// extern int ext2_first_free(uint32_t* b, int sz);
// extern uint32_t ext2_alloc_block(struct ext2_fs *f, int block_group);
// extern int ext2_write_indirect(struct ext2_fs *f, uint32_t indirect, uint32_t link, size_t block_num);
// extern uint32_t ext2_read_indirect(struct ext2_fs *f, uint32_t indirect, size_t block_num);

// /* file.c */
// extern int ext2_add_link(struct ext2_fs *f, int inode_num);
// extern size_t ext2_write_file(struct ext2_fs *f, int inode_num, int parent_dir, char* name, char* data, int mode, uint32_t n);
// extern size_t ext2_read_file(struct ext2_fs *f, struct ext2_inode* in, char* buf);
// extern size_t ext2_touch_file(struct ext2_fs *f, int parent, char* name, char* data, int mode, size_t n);

// /* dir.c */
// extern int ext2_add_child(struct ext2_fs *f, int parent_inode, int i_no, char* name, int type);
// extern int ext2_find_child(struct ext2_fs *f, const char* name, int dir_inode);
// extern struct ext2_dirent* ext2_create_dir(struct ext2_fs *f, char* name, int parent_inode);

// extern char* gen_file_perm_string(uint16_t x);
// extern void ls(struct ext2_fs *f, int inode_num);

// /* inode.c */
// //extern void ext2_read_inode(struct inode*);
// //extern void ext2_write_inode(struct ext2_fs *f, int inode_num, struct ext2_inode* i);
// //extern struct inode* ext2_alloc_inode(struct super_block *);
// //extern void ext2_free_inode(struct inode* inode);

// /* sync.c */
// extern struct super_block* ext2_mount(int dev);
// extern void sync(struct ext2_fs *f);
// extern void release_fs(struct ext2_fs *f);
// extern void acquire_fs(struct ext2_fs *f);
// extern int pathize(struct ext2_fs* f, char* path);


#endif

// in mem, as of kernel 4.8
 //69 struct ext2_sb_info {
 // 70         unsigned long s_frag_size;      /* Size of a fragment in bytes */
 // 71         unsigned long s_frags_per_block;/* Number of fragments per block */
 // 72         unsigned long s_inodes_per_block;/* Number of inodes per block */
 // 73         unsigned long s_frags_per_group;/* Number of fragments in a group */
 // 74         unsigned long s_blocks_per_group;/* Number of blocks in a group */
 // 75         unsigned long s_inodes_per_group;/* Number of inodes in a group */
 // 76         unsigned long s_itb_per_group;  /* Number of inode table blocks per group */
 // 77         unsigned long s_gdb_count;      /* Number of group descriptor blocks */
 // 78         unsigned long s_desc_per_blocmake
 k; /* Number of group descriptors per block */
 // 79         unsigned long s_groups_count;   /* Number of groups in the fs */
 // 80         unsigned long s_overhead_last;  /* Last calculated overhead */
 // 81         unsigned long s_blocks_last;    /* Last seen block count */
 // 82         struct buffer_head * s_sbh;     Buffer containing the super block */
 // 83         struct ext2_super_block * s_es; Pointer to the super block in the buffer */
//  84         struct buffer_head ** s_group_desc;
//  85         unsigned long  s_mount_opt;
//  86         unsigned long s_sb_block;
//  87         kuid_t s_resuid;
//  88         kgid_t s_resgid;
//  89         unsigned short s_mount_state;
//  90         unsigned short s_pad;
//  91         int s_addr_per_block_bits;
//  92         int s_desc_per_block_bits;
//  93         int s_inode_size;
//  94         int s_first_ino;
//  95         spinlock_t s_next_gen_lock;
//  96         u32 s_next_generation;
//  97         unsigned long s_dir_count;
//  98         u8 *s_debts;
//  99         struct percpu_counter s_freeblocks_counter;
// 100         struct percpu_counter s_freeinodes_counter;
// 101         struct percpu_counter s_dirs_counter;
// 102         struct blockgroup_lock *s_blockgroup_lock;
// 104         spinlock_t s_rsv_window_lock;
// 105         struct rb_root s_rsv_window_root;
// 106         struct ext2_reserve_window_node s_rsv_window_head;

// 115         spinlock_t s_lock;
// 116        
