#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>

#define BCTBX_VFS_OK           1   /* Successful result */

#define BCTBX_VFS_ERROR       -1   /* Some kind of disk I/O error occurred */


#define BCTBX_VFS_OPEN_READONLY         0x00000001  
#define BCTBX_VFS_OPEN_READWRITE        0x00000002  
#define BCTBX_VFS_OPEN_CREATE           0x00000004  
#define BCTBX_VFS_OPEN_DELETEONCLOSE    0x00000008  
#define BCTBX_VFS_OPEN_EXCLUSIVE        0x00000010  
/**
 */
typedef struct bc_io_methods bc_io_methods;
/**
 */
typedef struct bc_vfs_file bc_vfs_file;
struct bc_vfs_file {
	const struct bc_io_methods *pMethods;  /* Methods for an open file */
	int fd;                         	/* File descriptor */
	FILE* file;							/*File stream */
	int offset;							/*File offset used by lseek*/
	char* filename;
	uint64_t size;
};


/**
 */
struct bc_io_methods {
	int (*pFuncClose)(bc_vfs_file*, int* pErrSvd);
	int (*pFuncRead)(bc_vfs_file*, void*, int count, uint64_t offset,int* pErrSvd);
	int (*pFuncWrite)(bc_vfs_file*, const void*, int count, uint64_t offset, int* pErrSvd);
	int (*pFuncFileSize)(bc_vfs_file*);
	int (*pFuncGetLineFromFd)(bc_vfs_file*, char* s, int count);
	char*  (*pFuncFgets)(bc_vfs_file*, char* s, int count);
	int (*pFuncFprintf)(bc_vfs_file*, const char* s);
	int	(*pFuncSeek)(bc_vfs_file *pFile, uint64_t offset, int whence);
};


/**
 */
typedef struct bc_vfs bc_vfs;
struct bc_vfs {
	bc_vfs *pNext;      /* Next registered VFS */
	const char *vfsName;       /* Virtual file system name */
	bc_vfs_file* (*pFuncFopen)(bc_vfs* pVfs, const char *fName,  const char* mode, int* pErrSvd);
	// int (*pFuncDelete)(bc_vfs*, const char *vfsName, int syncDir);
	// int (*pFuncFullPathname)(bc_vfs*, const char *vfsName, int nOut, char *zOut);
	
};

bc_vfs *bc_create_vfs(void);
int bc_vfs_register(bc_vfs* pVfs, bc_vfs** pToVfs);
int bctbx_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset);
int bctbx_file_close(bc_vfs_file* pFile);
bc_vfs_file* bctbx_file_open(bc_vfs* pVfs, const char *fName,  const char* mode);
uint64_t bctbx_file_size(bc_vfs_file *pFile);
int bctbx_file_write(bc_vfs_file* pFile, const void *buf, int count, uint64_t offset);
int bctbx_file_fprintf(bc_vfs_file* pFile, uint64_t offset, const char* fmt, ...);
int bctbx_file_get_nxtline(bc_vfs_file* pFile, char*s , int maxlen);
int bctbx_file_seek(bc_vfs_file *pFile, uint64_t offset, int whence);


