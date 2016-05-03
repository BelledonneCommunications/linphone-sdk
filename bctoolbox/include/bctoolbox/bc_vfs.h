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
#define BC_VFS_ERROR        -1   /* error or missing file */

#define BC_VFS_IOERR       -1   /* Some kind of disk I/O error occurred */

#define BC_VFS_CANTOPEN    -1   /* Unable to open the file */


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
	int offset;

	
};


/**
 */
struct bc_io_methods {
	int (*xClose)(bc_vfs_file*);
	int (*xRead)(bc_vfs_file*, void*, int count, uint64_t offset);
	int (*xWrite)(bc_vfs_file*, const void*, int count, uint64_t offset);
	// int (*xTruncate)(bc_vfs_file*, uint64_t size);
	int (*xFileSize)(bc_vfs_file*, uint64_t *pSize);
	int (*xFgets)(bc_vfs_file*, char* s, int count);
	int (*xFprintf)(bc_vfs_file*, const char* s);
	int	(*xSeek)(bc_vfs_file *pFile, uint64_t offset, int whence);
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
int bc_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset);
int bc_file_close(bc_vfs_file* pFile);
bc_vfs_file* bc_file_open(bc_vfs* pVfs, const char *fName,  const char* mode);
int bc_file_size(bc_vfs_file *pFile, uint64_t *pSize);
int bc_file_write(bc_vfs_file* pFile, const void *buf, int count, uint64_t offset);
int bc_file_printf(bc_vfs_file* pFile, uint64_t offset, const char* fmt, ...);
char * bc_file_get_nxtline(bc_vfs_file* pFile, char*s , int maxlen);
int bc_file_seek(bc_vfs_file *pFile, uint64_t offset, int whence);


