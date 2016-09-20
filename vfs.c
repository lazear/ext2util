#include "ext2.h"
#include <stdint.h>
#include <assert.h>


typedef struct _vfs_inode {
	uint32_t i_no;
	uint16_t ref_count;		// Number of references


	uint16_t mode;			// Format of the file, and access rights
	uint16_t uid;			// User id associated with file
	uint16_t gid;			// Group ID
	uint32_t size;			// Size of file in bytes
	uint32_t atime;			// Last access time, POSIX
	uint32_t ctime;			// Creation time
	uint32_t mtime;			// Last modified time
	uint32_t dtime;			// Deletion time

//	vfs_file_ops vfo;


} vfs_inode;

typedef struct _vfs_file {

} file;

typedef struct _vfs_inode_operations {
	int (*link)		(vfs_inode*, vfs_inode*);
	int (*unlink)	(vfs_inode*, vfs_inode*);

} vfs_inode_ops;

typedef struct _vfs_file_operations {

	int (*read) 	(vfs_inode*, file*, char* buffer, int n);
	int (*write)	(vfs_inode*, file*, char* buffer, int n);
	int (*seek)		(vfs_inode*, file*, int offset);
	int (*open)		(vfs_inode*, file*);
	int (*close)	(vfs_inode*, file*);

} vfs_file_ops;


typedef struct _vfs_node_struct {

	char name;
	uint32_t inode_num;


} vfs_node;
