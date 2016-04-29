#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>

#define BC_VFS_OK           1   /* Successful result */
/* beginning-of-error-codes */
#define BC_VFS_ERROR        0   /* error or missing file */
#define BC_VFS_PERM         2   /* Access permission denied */
#define BC_VFS_ABORT        3   /* Callback routine requested an abort */
#define BC_VFS_NOMEM        5   /* A malloc() failed */
#define BC_VFS_READONLY     8   /* Attempt to write a readonly file */
#define BC_VFS_INTERRUPT    9   /* Operation terminated by interrupt()*/
#define BC_VFS_IOERR       10   /* Some kind of disk I/O error occurred */
#define BC_VFS_CORRUPT     11   /* The file disk image is malformed */
#define BC_VFS_NOTFOUND    12   /* Unknown opcode in sqlite3_file_control() */
#define BC_VFS_FULL        13   /* Insertion failed because file is full */
#define BC_VFS_CANTOPEN    -1   /* Unable to open the file */
#define BC_VFS_EMPTY       16   /* file is empty */

#define BC_VFS_DONE        101
	
#define BC_VFS_OPEN_READONLY         0x00000001  
#define BC_VFS_OPEN_READWRITE        0x00000002  
#define BC_VFS_OPEN_CREATE           0x00000004  
#define BC_VFS_OPEN_DELETEONCLOSE    0x00000008  
#define BC_VFS_OPEN_EXCLUSIVE        0x00000010  
#define BC_VFS_IOERR_READ              (BC_VFS_IOERR | (1<<8))
#define BC_VFS_IOERR_SHORT_READ        (BC_VFS_IOERR | (2<<8))
#define BC_VFS_IOERR_WRITE             (BC_VFS_IOERR | (3<<8))
#define BC_VFS_IOERR_FSTAT             (BC_VFS_IOERR | (7<<8))


/**
 */
typedef struct bc_io_methods bc_io_methods;
/**
 */
typedef struct bc_vfs_file bc_vfs_file;
struct bc_vfs_file {
	const struct bc_io_methods *pMethods;  /* Methods for an open file */
	int fd;                         /* File descriptor */
	FILE* file;

	
};


/**
 */
struct bc_io_methods {
	int (*xClose)(bc_vfs_file*);
	int (*xRead)(bc_vfs_file*, void*, int count, uint64_t offset);
	int (*xWrite)(bc_vfs_file*, const void*, int count, uint64_t offset);
	// int (*xTruncate)(bc_vfs_file*, uint64_t size);
	int (*xFileSize)(bc_vfs_file*, uint64_t *pSize);
	char* (*xFgets)(bc_vfs_file*, char* s, int count);
	int (*xFprintf)(bc_vfs_file*, const char* s);
};


/**
 */
typedef struct bc_vfs bc_vfs;
struct bc_vfs {
	bc_vfs *pNext;      /* Next registered VFS */
	
	const char *vfsName;       /* Virtual file system name */
	// int (*xOpen)(bc_vfs* pVfs, const char *fName, bc_vfs_file* pFile, int flags, char mode);
	// int (*xFopen)(bc_vfs* pVfs, const char *fName, bc_vfs_file** ppFile, const char* mode);
	bc_vfs_file* (*xFopen)(bc_vfs* pVfs, const char *fName,  const char* mode);
	// int (*xDelete)(bc_vfs*, const char *vfsName, int syncDir);
	// int (*xFullPathname)(bc_vfs*, const char *vfsName, int nOut, char *zOut);
	
};

bc_vfs *bc_create_vfs(void);
int bc_vfs_register(bc_vfs* pVfs, bc_vfs** pToVfs);


// void bc_vfs_add_opened(bc_vfs* pVfs, bc_vfs_file* pFile);
// void bc_vfs_delete_closed_file(bc_vfs* pVfs, bc_vfs_file* pFile);
// bc_vfs_file* bc_vfs_find_file(bc_vfs* pVfs, bc_vfs_file* pFile, int previous_or_next);


//int bc_vfs_unregister(bc_vfs* g);
	
int bc_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset);

int bc_file_close(bc_vfs_file* pFile);
bc_vfs_file* bc_file_open(bc_vfs* pVfs, const char *fName,  const char* mode);

int bc_file_size(bc_vfs_file *pFile, uint64_t *pSize);

