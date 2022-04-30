/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

struct open_file {
    struct vnode *vn;
    off_t fp;
    int flags;
    int count;
};

// of_table is an array of pointers to struct open_file
struct open_file *of_table[GLOBAL_OPEN_MAX];

// Prototypes for IN-KERNEL entry points for system call implementations.
int sys_open(userptr_t filename, int flags, mode_t mode, int *retval);
int sys_close(int fd);
int sys_read(int fd, userptr_t buf, size_t buflen, ssize_t *retval);
int sys_write(int fd, userptr_t buf, size_t buflen, ssize_t *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_lseek(int fd, uint64_t offset, int whence, off_t *retval);

// initialize the open file table
int oft_initialize(void);
// destroy the open file table
void oft_destroy(void);
// initialize the file descriptor table of the current process
int fdt_initialize(void);
// destroy the file descriptor table of the current process
void fdt_destroy(struct proc *proc);

#endif /* _FILE_H_ */
