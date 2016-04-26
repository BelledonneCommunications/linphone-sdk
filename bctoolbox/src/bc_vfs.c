#include "bctoolbox/bc_vfs.h"




/**
*/
bc_vfs *bc_vfs_find(bc_vfs* p, const char *zVfsName){
  bc_vfs *tmp = p;
  do{
    if (tmp->zName == zVfsName){
      return tmp;
    }
    tmp = tmp->pNext;

  }while(tmp != 0);
  return NULL;
}
/**
*/
int bc_vfs_register(bc_vfs* ptr, bc_vfs* g, int makeDflt){
  g = ptr;
  return 1;
}

/**
*/
int bc_vfs_unregister(bc_vfs* g){
  g = NULL;
  return 1;
}
/*
** When using this VFS, the bc_vfs_file* handles that SQLite uses are
** actually pointers to instances of type DemoFile.
*/
typedef struct DemoFile DemoFile;
struct DemoFile {
  bc_vfs_file base;                   /* Base class. Must be first. */
  int fd;                         /* File descriptor */

  // char *aBuffer;                  /* Pointer to malloc'd buffer */
  // int nBuffer;                    /* Valid bytes of data in zBuffer */
  // size_t iBufferOfst;             /* Offset in file of zBuffer[0] */
};


/*
** Close a file.
*/
static int bcClose(bc_vfs_file *pFile){
  DemoFile *p = (DemoFile*)pFile;
  close(p->fd);
  return 0;
}


/*
** Read data from a file.
*/
static int bcRead(bc_vfs_file *pFile, void *zBuf, int iAmt, uint64_t iOfst){
  DemoFile *p = (DemoFile*)pFile;
  off_t ofst;                     /* Return value from lseek() */
  int nRead;                      /* Return value from read() */
  int rc;                         /* Return code from bcFlushBuffer() */

  ofst = lseek(p->fd, iOfst, SEEK_SET);
  if( ofst!=iOfst ){
    return BC_VFS_IOERR_READ;
  }
  nRead = read(p->fd, zBuf, iAmt);

  if( nRead==iAmt ){
    return BC_VFS_OK;
  }else if( nRead>=0 ){
    return BC_VFS_IOERR_SHORT_READ;
  }

  return BC_VFS_IOERR_READ;
}

/*
** Write directly to the file passed as the first argument. Even if the
** file has a write-buffer (DemoFile.aBuffer), ignore it.

  DemoFile *p,                    File handle 
  const void *zBuf,               Buffer containing data to write 
  int iAmt,                       Size of data to write in bytes 
  uint64_t iOfst              File offset to write to 
*/
static int bcDirectWrite(DemoFile *p, const void *zBuf, int iAmt, uint64_t iOfst ){
  off_t ofst;                     /* Return value from lseek() */
  size_t nWrite;                  /* Return value from write() */

  ofst = lseek(p->fd, iOfst, SEEK_SET);
  if( ofst!=iOfst ){
    return BC_VFS_IOERR_WRITE;
  }

  nWrite = write(p->fd, zBuf, iAmt);
  if( nWrite!=iAmt ){
    return BC_VFS_IOERR_WRITE;
  }

  return BC_VFS_OK;
}
/*
** Write data to a crash-file.
*/
static int bcWrite(bc_vfs_file *pFile, const void *zBuf, int iAmt, uint64_t iOfst){
  DemoFile *p = (DemoFile*)pFile;
  
  // if( 

      /* If the buffer is full, or if this data is not being written directly
      ** following the data already buffered, flush the buffer. Flushing
      ** the buffer is a no-op if it is empty.  
      */
      
  // }else{
    return bcDirectWrite(p, zBuf, iAmt, iOfst);
  // }

  return BC_VFS_OK;
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int bcFileSize(bc_vfs_file *pFile, uint64_t *pSize){
  DemoFile *p = (DemoFile*)pFile;
  int rc;                         /* Return code from fstat() call */
  struct stat sStat;              /* Output of fstat() call */


  rc = fstat(p->fd, &sStat);
  if( rc!=0 ) return BC_VFS_IOERR_FSTAT;
  *pSize = sStat.st_size;
  return BC_VFS_OK;
}
/*
** Open a file handle.
  bc_vfs *pVfs                  VFS 
  const char *zName             File to open, or 0 for a temp file 
  bc_vfs_file *pFile             Pointer to DemoFile struct to populate 
  int flags                     flags to pass to open() call 
  char mode  
*/
static int bcOpen(bc_vfs *pVfs, const char *zName, bc_vfs_file *pFile, int flags, mode_t mode){
  static const bc_io_methods bcio = {
      bcClose,                    /* xClose */
      bcRead,                     /* xRead */
      bcWrite,                    /* xWrite */
      // bcTruncate,                 /* xTruncate */
      bcFileSize,                 /* xFileSize */
  };

  DemoFile *p = (DemoFile*)pFile; /* Populate this structure */             
  // char *aBuf = 0;
  int oflags = 0;                 /* flags to pass to open() call */

  if( zName==0 ){
    return BC_VFS_IOERR;
  }

  if( flags&BC_VFS_OPEN_EXCLUSIVE ) oflags |= O_EXCL;
  if( flags&BC_VFS_OPEN_CREATE )    oflags |= O_CREAT;
  if( flags&BC_VFS_OPEN_READONLY )  oflags |= O_RDONLY;
  if( flags&BC_VFS_OPEN_READWRITE ) oflags |= O_RDWR;

  memset(p, 0, sizeof(DemoFile));
  p->fd = open(zName, oflags, mode);
  if( p->fd<0 ){
    return BC_VFS_CANTOPEN;
  }
  // p->aBuffer = aBuf;

  p->base.pMethods = &bcio;
  return BC_VFS_OK;
}


/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_demovfs(), 0);
*/
bc_vfs *bc_demovfs(void){
  static bc_vfs bc_demovfs = {
    sizeof(DemoFile),             /* szOsFile */
    MAXPATHNAME,                  /* mxPathname */
    0,                            /* pNext */
    "demo",                       /* zName */
//    0,                            /* pAppData */
    bcOpen,                       /* xOpen */
    // bcDelete,                     /* xDelete */
    // bcFullPathname,               /* xFullPathname */
  };
  return &bc_demovfs;
}