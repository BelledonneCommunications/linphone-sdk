#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "sqlite3.h"

#define BCTBX_VFS_OK           1   /* Successful result */

#define BCTBX_VFS_ERROR       -1   /* Some kind of disk I/O error occurred */


#define BCTBX_VFS_OPEN_READONLY         0x00000001  
#define BCTBX_VFS_OPEN_READWRITE        0x00000002  
#define BCTBX_VFS_OPEN_CREATE           0x00000004  
#define BCTBX_VFS_OPEN_DELETEONCLOSE    0x00000008  
#define BCTBX_VFS_OPEN_EXCLUSIVE        0x00000010  

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 512
/**
 */
typedef struct bc_io_methods bc_io_methods;
/**
 */
typedef struct bc_vfs_file bc_vfs_file;
struct bc_vfs_file {
	sqlite3_file base;              /* Base class. Must be first. */
	// const struct bc_io_methods *pMethods;  /* Methods for an open file */
	int fd;                         	/* File descriptor */
	FILE* file;							/*File stream */
	sqlite3_int64 offset;				/*File offset used by lseek*/
	char* filename;
	uint64_t size;
};


/**
 */
struct bc_io_methods {
	int iVersion;
	int (*xClose)(sqlite3_file* p);
	int (*xRead)(sqlite3_file* p, void*, int count, sqlite_int64 offset);
	int (*xWrite)(sqlite3_file* p, const void*, int count, sqlite_int64 offset);
	int (*xFileSize)(sqlite3_file* p, sqlite_int64 *pSize);
	int (*xGetLineFromFd)(sqlite3_file* p, char* s, int count);
	char*  (*xFgets)(sqlite3_file* p, char* s, int count);
	int (*xFprintf)(sqlite3_file* p, const char* s);
	int	(*xSeek)(sqlite3_file* p, sqlite_int64 offset, int whence);
};


/**
 */
typedef struct bc_vfs bc_vfs;
struct bc_vfs {
	bc_vfs *pNext;      /* Next registered VFS */
	const char *vfsName;       /* Virtual file system name */
	int (*xOpen)(sqlite3_vfs* pVfs, const char *fName, sqlite3_file *pFile,int flags, int *pOutFlags);
	// int (*xDelete)(bc_vfs*, const char *vfsName, int syncDir);
	// int (*xFullPathname)(bc_vfs*, const char *vfsName, int nOut, char *zOut);
	
};

bc_vfs *bc_create_vfs(void);
sqlite3_vfs *sqlite3_bctbx_vfs(void);
int bc_vfs_register(bc_vfs* pVfs, bc_vfs** pToVfs);
int bctbx_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset);
int bctbx_file_close(bc_vfs_file* pFile);
bc_vfs_file* bctbx_file_open(sqlite3_vfs* pVfs, const char *fName,  const char* mode);
uint64_t bctbx_file_size(bc_vfs_file *pFile, sqlite_int64 *pSize);
int bctbx_file_write(bc_vfs_file* pFile, const void *buf, int count, uint64_t offset);
int bctbx_file_fprintf(bc_vfs_file* pFile, uint64_t offset, const char* fmt, ...);
int bctbx_file_get_nxtline(bc_vfs_file* pFile, char*s , int maxlen);
int bctbx_file_seek(bc_vfs_file *pFile, uint64_t offset, int whence);


