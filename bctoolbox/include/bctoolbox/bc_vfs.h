#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 

#include <sys/stat.h> 

#define BC_VFS_OK           0   /* Successful result */
/* beginning-of-error-codes */
#define BC_VFS_ERROR        1   /* error or missing file */
#define BC_VFS_PERM         2   /* Access permission denied */
#define BC_VFS_ABORT        3   /* Callback routine requested an abort */
#define BC_VFS_NOMEM        5   /* A malloc() failed */
#define BC_VFS_READONLY     8   /* Attempt to write a readonly file */
#define BC_VFS_INTERRUPT    9   /* Operation terminated by interrupt()*/
#define BC_VFS_IOERR       10   /* Some kind of disk I/O error occurred */
#define BC_VFS_CORRUPT     11   /* The file disk image is malformed */
#define BC_VFS_NOTFOUND    12   /* Unknown opcode in sqlite3_file_control() */
#define BC_VFS_FULL        13   /* Insertion failed because file is full */
#define BC_VFS_CANTOPEN    14   /* Unable to open the file */
#define BC_VFS_EMPTY       16   /* file is empty */
#define BC_VFS_TOOBIG      18   /* String or BLOB exceeds size limit */
#define BC_VFS_CONSTRAINT  19   /* Abort due to constraint violation */
#define BC_VFS_MISMATCH    20   /* Data type mismatch */
#define BC_VFS_MISUSE      21   /* Library used incorrectly */
#define BC_VFS_NOLFS       22   /* Uses OS features not supported on host */
#define BC_VFS_AUTH        23   /* Authorization denied */
#define BC_VFS_FORMAT      24   /* Auxiliary database format error */
#define BC_VFS_RANGE       25   /* 2nd parameter to sqlite3_bind out of range */
#define BC_VFS_WARNING     28   /* Warnings from sqlite3_log() */
#define BC_VFS_DONE        101  /* sqlite3_step() has finished executing */

#define BC_VFS_OPEN_READONLY         0x00000001  /* Ok for sqlite3_open_v2() */
#define BC_VFS_OPEN_READWRITE        0x00000002  /* Ok for sqlite3_open_v2() */
#define BC_VFS_OPEN_CREATE           0x00000004  /* Ok for sqlite3_open_v2() */
#define BC_VFS_OPEN_DELETEONCLOSE    0x00000008  /* VFS only */
#define BC_VFS_OPEN_EXCLUSIVE        0x00000010  /* VFS only */
#define BC_VFS_IOERR_READ              (BC_VFS_IOERR | (1<<8))
#define BC_VFS_IOERR_SHORT_READ        (BC_VFS_IOERR | (2<<8))
#define BC_VFS_IOERR_WRITE             (BC_VFS_IOERR | (3<<8))
#define BC_VFS_IOERR_FSTAT             (BC_VFS_IOERR | (7<<8))


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
  const struct bc_io_methods *pMethods;  /* Methods for an open file */
};
/**
*/
struct bc_io_methods {
  int (*xClose)(bc_vfs_file*);
  int (*xRead)(bc_vfs_file*, void*, int iAmt, uint64_t iOfst);
  int (*xWrite)(bc_vfs_file*, const void*, int iAmt, uint64_t iOfst);
  // int (*xTruncate)(bc_vfs_file*, uint64_t size);
  int (*xFileSize)(bc_vfs_file*, uint64_t *pSize);
};


/**
*/
typedef struct bc_vfs bc_vfs;
struct bc_vfs {
  int szOsFile;            /* Size of subclassed bc_vfs_file */
  int mxPathname;          /* Maximum file pathname length */
  bc_vfs *pNext;      /* Next registered VFS */
  const char *zName;       /* Name of this virtual file system */
  int (*xOpen)(bc_vfs*, const char *zName, bc_vfs_file*, int flags, char mode);
  // int (*xDelete)(bc_vfs*, const char *zName, int syncDir);
  // int (*xFullPathname)(bc_vfs*, const char *zName, int nOut, char *zOut);

};
bc_vfs *bc_vfs_find(bc_vfs* p, const char *zVfsName);
bc_vfs *bc_demovfs(void);
int bc_vfs_register(bc_vfs* ptr,bc_vfs* g, int makeDflt);
int bc_vfs_unregister(bc_vfs* g);




