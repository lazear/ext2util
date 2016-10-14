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