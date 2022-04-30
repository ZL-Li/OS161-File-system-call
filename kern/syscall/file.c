#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */

int oft_initialize(void) {

    int i;
    // of_table is an array of pointers to struct open_file
    // first malloc the space
    for (i = 0; i < GLOBAL_OPEN_MAX; i++) {
        of_table[i] = kmalloc(sizeof(struct open_file));
        if (of_table[i] == NULL) {
            // out of memory
            return ENOMEM;
        }
    }

    // initialize each pointer to NULL
    for (i = 0; i < GLOBAL_OPEN_MAX; i++) {
        of_table[i] = NULL;
    }
    return 0;
}


void oft_destroy(void) {

    int i;

    for (i = 0; i < GLOBAL_OPEN_MAX; i++) {
        if (of_table[i] != NULL) {
            // destroy open file
            vfs_close(of_table[i]->vn);
            kfree(of_table[i]);
            of_table[i] = NULL;
        }
    }
}


int fdt_initialize(void) {
    
    int i;
    char stdout[] = "con:";
    char stderr[] = "con:";
    int oft_index1 = -1, oft_index2 = -1;
    int result;

    // initialize fd_table with -1
	for (int i = 0; i < OPEN_MAX; i++) {
		curproc->fd_table[i] = -1;
	}

    // now we attach fd_table[1] and fd_table[2] to console device
    // same as part of sys_open

    // now we try to find 2 free space in open_file_table and
    // keep the index of the empty lot into variable oft_index1 and oft_index2
    for (i = 0; i < GLOBAL_OPEN_MAX; i++) {
        if (of_table[i] == NULL) {
            oft_index1 = i;
            break;
        }
    }
    if (oft_index1 == -1) {
        //no free space left in open_file_table
        return ENFILE;
    }

    for (i = oft_index1 + 1; i < GLOBAL_OPEN_MAX; i++) {
        if (of_table[i] == NULL) {
            oft_index2 = i;
            break;
        }
    }
    if (oft_index2 == -1) {
        //no free space left in open_file_table
        return ENFILE;
    }

    struct vnode *vn1;
    struct vnode *vn2;

    // vfs_open
    result = vfs_open(stdout, O_WRONLY, 0, &vn1);
    if (result) {
        return result;
    }

    result = vfs_open(stderr, O_WRONLY, 0, &vn2);
    if (result) {
        return result;
    }

    // initialize the open file
    struct open_file *of1 = kmalloc(sizeof(struct open_file));
    if (of1 == NULL) {
        // out of memory
        return ENOMEM;
    }
    of1->vn = vn1;
    of1->fp = 0;
    of1->flags = O_WRONLY;
    of1->count = 1;

    struct open_file *of2 = kmalloc(sizeof(struct open_file));
    if (of2 == NULL) {
        // out of memory
        return ENOMEM;
    }
    of2->vn = vn2;
    of2->fp = 0;
    of2->flags = O_WRONLY;
    of2->count = 1;

    // update of_table
    of_table[oft_index1] = of1;
    of_table[oft_index2] = of2;
    // update fd_table of current process
    curproc->fd_table[1] = oft_index1;
    curproc->fd_table[2] = oft_index2;

    return 0;
}


void fdt_destroy(struct proc *proc) {

    int i;

    for (i = 0; i < OPEN_MAX; i++) {
        if (proc->fd_table[i] != -1) {
            // if open file already be destroyed
            if (of_table[proc->fd_table[i]] == NULL) {
                proc->fd_table[i] = -1;
            }
            // else destroy open file
            else {
                vfs_close(of_table[proc->fd_table[i]]->vn);
                kfree(of_table[proc->fd_table[i]]);
                of_table[proc->fd_table[i]] = NULL;
                proc->fd_table[i] = -1;
            }
        }
    }
}


int sys_open(userptr_t filename, int flags, mode_t mode, int *retval) {

    char copied_filename[PATH_MAX];
    int result;
    size_t *got = NULL;
    int fd_index = -1, oft_index = -1;
    int i;

    // copy a string of filename with length PATH_MAX 
    // from a user-space address to a kernel-space address
    // return the actual length of string found in GOT
    result = copyinstr(filename, copied_filename, PATH_MAX, got);
    if (result) {
        return result;
    }

    // now we try to find a free space in fd_table of current process and
    // keep the index of the empty lot into variable fd_index
    for (i = 0; i < OPEN_MAX; i++) {
        if (curproc->fd_table[i] == -1) {
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        //no free space left in fd_table of current process
        return EMFILE;
    }

    // now we try to find a free space in open_file_table and
    // keep the index of the empty lot into variable oft_index
    for (i = 0; i < GLOBAL_OPEN_MAX; i++) {
        if (of_table[i] == NULL) {
            oft_index = i;
            break;
        }
    }
    if (oft_index == -1) {
        //no free space left in open_file_table
        return ENFILE;
    }

    struct vnode *vn;

    // vfs_open
    result = vfs_open(copied_filename, flags, mode, &vn);
    if (result) {
        return result;
    }

    // initialize the open file
    struct open_file *of = kmalloc(sizeof(struct open_file));
    if (of == NULL) {
        // out of memory
        return ENOMEM;
    }
    of->vn = vn;
    of->fp = 0;
    of->flags = flags;
    of->count = 1;

    // update of_table
    of_table[oft_index] = of;
    // update fd_table of current process
    curproc->fd_table[fd_index] = oft_index;
    // push fd_index (file handle) back to user-level
    *retval = fd_index;

    return 0;
}


int sys_close(int fd) {

    // first we check if fd is valid
    if (fd < 0 || fd >= OPEN_MAX) {
        return EINVAL;
    }

    int oft_index = curproc->fd_table[fd];

    if (oft_index == -1 || of_table[oft_index] == NULL) {
        return EINVAL;
    }

    // open file count--
    of_table[oft_index]->count--;

    // if no process attached open file, close the file and update of_table
    if (of_table[oft_index]->count == 0) {
        vfs_close(of_table[oft_index]->vn);
        kfree(of_table[oft_index]);
        of_table[oft_index] = NULL;
    }

    // update the fd_table
    curproc->fd_table[fd] = -1;
    return 0;
}


int sys_read(int fd, userptr_t buf, size_t buflen, ssize_t *retval) {

    int result;

    // first we check if fd is valid
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    
    int oft_index = curproc->fd_table[fd];

    if (oft_index == -1) {
        return EBADF;
    }

    struct open_file *of = of_table[oft_index];

    if (of == NULL || of->flags == O_WRONLY) {
        return EBADF;
    }

    struct uio *uio;
    struct iovec *iovec;

    uio = kmalloc(sizeof(struct uio));
    if (uio == NULL) {
        // out of memory
        return ENOMEM;
    }
    iovec = kmalloc(sizeof(struct iovec));
    if (iovec == NULL) {
        // out of memory
        return ENOMEM;
    }

    // helper function
    uio_uinit(iovec, uio, buf, buflen, of->fp, UIO_READ);

    result = VOP_READ(of->vn, uio);
    if(result) {
        return result;
    }

    // update the file pointer
    of->fp = uio->uio_offset;
    // return bytes read
    *retval = buflen - uio->uio_resid;

    kfree(uio);
    uio = NULL;
    kfree(iovec);
    iovec = NULL;

    return 0;
}

int sys_write(int fd, userptr_t buf, size_t buflen, ssize_t *retval) {

    int result;

    // first we check if fd is valid
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    
    int oft_index = curproc->fd_table[fd];

    if (oft_index == -1) {
        return EBADF;
    }

    struct open_file *of = of_table[oft_index];

    if (of == NULL || of->flags == O_RDONLY) {
        return EBADF;
    }

    struct uio *uio;
    struct iovec *iovec;

    uio = kmalloc(sizeof(struct uio));
    if (uio == NULL) {
        // out of memory
        return ENOMEM;
    }
    iovec = kmalloc(sizeof(struct iovec));
    if (iovec == NULL) {
        // out of memory
        return ENOMEM;
    }

    // helper function
    uio_uinit(iovec, uio, buf, buflen, of->fp, UIO_WRITE);

    result = VOP_WRITE(of->vn, uio);
    if(result) {
        return result;
    }

    // update the file pointer
    of->fp = uio->uio_offset;
    // return bytes written
    *retval = buflen - uio->uio_resid;

    kfree(uio);
    uio = NULL;
    kfree(iovec);
    iovec = NULL;

    return 0;
}


int sys_lseek(int fd, uint64_t offset, int whence, off_t *retval) {

    int result;

    // first we check if fd is valid
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    
    int oft_index = curproc->fd_table[fd];

    if (oft_index == -1) {
        return EBADF;
    }

    struct open_file *of = of_table[oft_index];

    if (of == NULL) {
        return EBADF;
    }

    // then we check if whence is valid
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        return EINVAL;
    }

    // then we check if it is seekable
    if (! VOP_ISSEEKABLE(of->vn)){
        return ESPIPE;
    }

    // we obtain the file size from VOP_STAT
    struct stat *stat = kmalloc(sizeof(struct stat));
    if (stat == NULL) {
        // out of memory
        return ENOMEM;
    }

    result = VOP_STAT(of->vn, stat);
    if (result) {
        return result;
    }

    off_t fp = 0;

    // seek from the beginning
    if (whence == SEEK_SET) {
        fp = offset;
    }
    // seek from the current position
    else if (whence == SEEK_CUR) {
        fp = of->fp + offset;
    }
    // seek from the ending
    else if (whence == SEEK_END) {
        fp = stat->st_size + offset;
    }

    if (fp < 0) {
        return EINVAL;
    }

    kfree(stat);

    // update the new file pointer
    of->fp = fp;
    *retval = fp;

    return 0;
}


int sys_dup2(int oldfd, int newfd, int *retval) {

    // first we check if oldfd is valid
    if (oldfd < 0 || oldfd >= OPEN_MAX) {
        return EBADF;
    }
    
    int old_oft_index = curproc->fd_table[oldfd];

    if (old_oft_index == -1) {
        return EBADF;
    }

    struct open_file *old_of = of_table[old_oft_index];

    if (old_of == NULL) {
        return EBADF;
    }

    // then we check if newfd is valid
    if (newfd < 0 || newfd >= OPEN_MAX) {
        return EBADF;
    }
    
    int new_oft_index = curproc->fd_table[newfd];

    // have to discuss this situation to avoid bugs behind
    if (oldfd == newfd) {
        *retval = newfd;
        return 0;
    }

    // reduce the reference counts of newfd
    if (new_oft_index != -1) {
        of_table[new_oft_index]->count--;
        if (of_table[new_oft_index]->count == 0) {
            vfs_close(of_table[new_oft_index]->vn);
            kfree(of_table[new_oft_index]);
            of_table[new_oft_index] = NULL;
        }
    }

    // update newfd in the fd_table 
    curproc->fd_table[newfd] = old_oft_index;

    // increase the reference counts of oldfd
    old_of->count++;

    *retval = newfd;
    return 0;
}

