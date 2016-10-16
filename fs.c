#include "fs.h"
#include "errno.h"

int register_filesystem(struct file_system_type* fs) {
	struct file_system_type** tmp;
	if (!fs)
		return -EINVAL;
	if (fs->next)
		return -EBUSY;
	for (tmp = &file_systems; *tmp; tmp = &(*tmp)->next)
		if (strcmp((*tmp)->name, fs->name) == 0)
			return -EBUSY;	/* File system type already registered */
	*tmp = fs;
	return 0;
}

int unregister_filesystem(struct file_system_type* fs) {
	struct file_system_type** tmp;
	if (!fs)
		return -EINVAL;
	for (tmp = &file_systems; *tmp; tmp = &(*tmp)->next) {
		if (*tmp == fs) {
			*tmp = fs->next;
			fs->next = NULL;
			return 0;	/* File system type already registered */
		}
	}
	return -EINVAL;
}

void lock_inode(struct inode* inode) {}
void unlock_inode(struct inode* inode) {}

struct inode* get_empty_inode() {
	return malloc(sizeof(struct inode));
}

/* This should search the cached inodes list for a  match. 
If no match is available, get an empty inode and read it from disk */
struct inode* get_inode(struct super_block* sb, int i_no) {
	/* For now, we're just going to allocate a new empty inode, and read it */
	struct inode* inode = get_empty_inode();
	inode->i_ino = i_no;
	inode->i_dev = sb->s_dev;
	inode->i_sb = sb;

	read_inode(inode);
	return inode;

}

/* VFS abstraction for reading an inode. Find link to inode's vfs super_block,
and then use the super_block operations table to read the physical inode */
void read_inode(struct inode* inode){
	lock_inode(inode);
	if (inode->i_sb && inode->i_sb->s_ops && inode->i_sb->s_ops->read_inode)
		inode->i_sb->s_ops->read_inode(inode);
	else 
		printf("read_inode(): missing superblock information\n");
	unlock_inode(inode);
}

void write_inode(struct inode* inode){
	lock_inode(inode);
	if (inode->i_sb && inode->i_sb->s_ops && inode->i_sb->s_ops->write_inode)
		inode->i_sb->s_ops->write_inode(inode);
	unlock_inode(inode);
}