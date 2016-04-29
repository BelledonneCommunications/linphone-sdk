#include "bctoolbox/bc_vfs.h"
#include <stdbool.h>



/**
 */
bc_vfs *bc_vfs_find(bc_vfs* p, const char *zVfsName){
	bc_vfs *tmp = p;
	do{
		if (tmp->vfsName == zVfsName){
			return tmp;
		}
		tmp = tmp->pNext;
		
	}while(tmp != 0);
	return NULL;
}
/**
 */
int bc_vfs_register(bc_vfs* pVfs, bc_vfs** pToVfs){
	int ret = 1;
	if (pVfs != NULL){
		*pToVfs = pVfs;
	}
	else{
			ret = 0;
	}
	return ret;
}

bool bc_vfs_init(bc_vfs* pVfs, bc_vfs* pToVfs, bc_vfs_file* pToFile){
	bool ret = 1;
	bc_vfs_file *pFile = (bc_vfs_file*)calloc(sizeof(bc_vfs_file),1);
	if (pFile != NULL){
		pToFile = pFile;
	}
	else{ 
		ret = 0;
	}	
	if (pToVfs != NULL)
			pToVfs = pVfs;
	else{
		ret = 0;
	}
	return ret;
}

/**
 */





/*
 ** Close a file.
 */
static int bcClose(bc_vfs_file *pFile){

	/*The file descriptor is not dup'ed, and will be closed when the stream created by fdopen() is closed
	The fclose() function flushes the stream pointed to by fp (writing any buffered output data using fflush(3))
	 and closes the underlying file descriptor. */

	fclose(pFile->file);
	// pFile->fd = 0;
	// pFile->file = NULL;
	free(pFile);
	pFile = NULL;

	return 0;
}


/*
 ** Read data from a file.
 */
static int bcRead(bc_vfs_file *pFile, void *buf, int count, uint64_t offset){
	off_t ofst;                     /* Return value from lseek() */
	int nRead;                      /* Return value from read() */
	//  int rc;                         /* Return code from bcFlushBuffer() */
	
	ofst = lseek(pFile->fd, offset, SEEK_SET);
	if( ofst!=offset ){
		return BC_VFS_IOERR_READ;
	}
	nRead = read(pFile->fd, buf, count);
	
	if( nRead==count ){
		return BC_VFS_OK;
	}else if( nRead>=0 ){
		return BC_VFS_IOERR_SHORT_READ;
	}
	
	return BC_VFS_IOERR_READ;
}

/*
 ** Write directly to the file passed as the first argument.
 
 bc_vfs_file *p,                    File handle
 const void *buf,               Buffer containing data to write
 int count,                       Size of data to write in bytes
 uint64_t offset              File offset to write to
 */
static int bcDirectWrite(bc_vfs_file *p, const void *buf, int count, uint64_t offset ){
	off_t ofst;                     /* Return value from lseek() */
	size_t nWrite;                  /* Return value from write() */
	
	ofst = lseek(p->fd, offset, SEEK_SET);
	if( ofst!=offset ){
		return BC_VFS_IOERR_WRITE;
	}
	
	nWrite = write(p->fd, buf, count);
	if( nWrite!=count ){
		return BC_VFS_IOERR_WRITE;
	}
	
	return BC_VFS_OK;
}
/*
 ** Write data to a crash-file.
 */
static int bcWrite(bc_vfs_file *pFile, const void *buf, int count, uint64_t offset){

	
	/* If the buffer is full, or if this data is not being written directly
	 ** following the data already buffered, flush the buffer. Flushing
	 ** the buffer is a no-op if it is empty.
	 */
	
	// }else{
	return bcDirectWrite(pFile, buf, count, offset);
	// }
	
	return BC_VFS_OK;
}

/*
 ** Write the size of the file in bytes to *pSize.
 */
static int bcFileSize(bc_vfs_file *pFile, uint64_t *pSize){

	int rc;                         /* Return code from fstat() call */
	struct stat sStat;              /* Output of fstat() call */
	
	
	rc = fstat(pFile->fd, &sStat);
	if( rc!=0 ) return BC_VFS_IOERR_FSTAT;
	*pSize = sStat.st_size;
	return BC_VFS_OK;
}

static char* bcFgets(bc_vfs_file* pFile, char* s, int count){
	if (pFile->file != NULL){
		return fgets(s, count, pFile->file);
	}
	return NULL;
}

static int bcFprintf(bc_vfs_file* pFile, const char* s){
	if (pFile->file != NULL){
		return fprintf(pFile->file, s);
	}
	return 0;
}

static int set_flags(const char* mode){
	int flags;
	int oflags = 0;                 /* flags to pass to open() call */
	if (strcmp(mode, "r") == 0) {
  		flags =  O_RDONLY;
	} 
	if ((strcmp(mode, "r+") == 0) || (strcmp(mode, "w+") == 0)) {
		flags =  O_RDWR;
  // do something
	} 
	if(strcmp(mode, "w") == 0) {
  		flags =  O_WRONLY;
	}
	oflags = flags | O_CREAT;
	if( flags&BC_VFS_OPEN_EXCLUSIVE ) oflags |= O_EXCL;
	if( flags&BC_VFS_OPEN_CREATE )    oflags |= O_CREAT;
	if( flags&BC_VFS_OPEN_READONLY )  oflags |= O_RDONLY;
	if( flags&BC_VFS_OPEN_READWRITE ) oflags |= O_RDWR;

	return oflags;
}

static bc_vfs_file* bcFopen(bc_vfs *pVfs, const char *fName, const char *mode){
	static const bc_io_methods bcio = {
		bcClose,                    /* xClose */
		bcRead,                     /* xRead */
		bcWrite,                    /* xWrite */
		// bcTruncate,                 /* xTruncate */
		bcFileSize,                 /* xFileSize */
		bcFgets,
		bcFprintf,
	};
	bc_vfs_file* pFile = (bc_vfs_file*)calloc(sizeof(bc_vfs_file),1);
	if (pFile == NULL){
		return BC_VFS_NOMEM ;
	}

	
	if( fName==0 ){
		return BC_VFS_IOERR;
	}

	memset(pFile, 0, sizeof(bc_vfs_file));
	int oflags = 0;
	oflags =set_flags(mode);
	pFile->fd = open(fName, oflags, S_IRUSR | S_IWUSR);
	if( pFile->fd<0 ){
		return BC_VFS_CANTOPEN;
	}
	pFile->file = fdopen(pFile->fd, mode);
	if( pFile->file == NULL ){
		return BC_VFS_CANTOPEN;
	}
	// pFile->aBuffer = aBuf;
	
	pFile->pMethods = &bcio;
	return pFile;
}


// static int bcFopen(bc_vfs *pVfs, const char *fName, bc_vfs_file **ppFile, const char *mode){
// 	static const bc_io_methods bcio = {
// 		bcClose,                    /* xClose */
// 		bcRead,                     /* xRead */
// 		bcWrite,                    /* xWrite */
// 		// bcTruncate,                 /* xTruncate */
// 		bcFileSize,                 /* xFileSize */
// 		bcFgets,
// 		bcFprintf,
// 	};
// 	bc_vfs_file* pFile = *ppFile;
// 	if (pFile == NULL){
// 		pFile = (bc_vfs_file*)calloc(sizeof(bc_vfs_file),1);
// 		if (pFile == NULL){
// 			return BC_VFS_NOMEM ;
// 		}
// 	} 


	
// 	if( fName==0 ){
// 		return BC_VFS_IOERR;
// 	}

// 	memset(pFile, 0, sizeof(bc_vfs_file));
// 	int oflags = 0;
// 	oflags =set_flags(mode);
// 	pFile->fd = open(fName, oflags, S_IRUSR | S_IWUSR);
// 	if( pFile->fd<0 ){
// 		return BC_VFS_CANTOPEN;
// 	}
// 	pFile->file = fdopen(pFile->fd, mode);
// 	if( pFile->file == NULL ){
// 		return BC_VFS_CANTOPEN;
// 	}
// 	// pFile->aBuffer = aBuf;
	
// 	pFile->pMethods = &bcio;
// 	return pFile->fd;
// }
/*+
 ** Open a file handle.
 bc_vfs *pVfs                  VFS
 const char *fName             File to open.
 bc_vfs_file *pFile             Pointer to vfs_file struct to populate
 int flags                     flags to pass to open() call
 char mode
 */
// static int bcOpen(bc_vfs *pVfs, const char *fName, bc_vfs_file *pFile, int flags, mode_t mode){
// 	static const bc_io_methods bcio = {
// 		bcClose,                    /* xClose */
// 		bcRead,                     /* xRead */
// 		bcWrite,                    /* xWrite */
// 		// bcTruncate,                 /* xTruncate */
// 		bcFileSize,                 /* xFileSize */
// 		bcFgets,
// 	};
// 	int oflags = 0;                 /* flags to pass to open() call */

// 	// char *aBuf = 0;
	
	
// 	if( fName==0 ){
// 		return BC_VFS_IOERR;
// 	}
// 	if( flags&BC_VFS_OPEN_EXCLUSIVE ) oflags |= O_EXCL;
// 	if( flags&BC_VFS_OPEN_CREATE )    oflags |= O_CREAT;
// 	if( flags&BC_VFS_OPEN_READONLY )  oflags |= O_RDONLY;
// 	if( flags&BC_VFS_OPEN_READWRITE ) oflags |= O_RDWR;
// 	memset(pFile, 0, sizeof(bc_vfs_file));
// 	pFile->fd = open(fName, oflags, mode);
// 	if( pFile->fd<0 ){
// 		return BC_VFS_CANTOPEN;
// 	}
// 	// pFile->aBuffer = aBuf;
	
// 	pFile->pMethods = &bcio;
// 	return pFile->fd;
// }


/*
 ** This function returns a pointer to the VFS implemented in this file.
 **
 */
bc_vfs *bc_create_vfs(void){
	static bc_vfs bcVfs = {
		0,                            /* pNext */
		"bc_vfs",                     /* vfsName */
	//	    0,                        /* pAppData */
	//	bcOpen,                       /* xOpen */
		bcFopen,						/*xFopen */
		// bcDelete,                     /* xDelete */
		// bcFullPathname,               /* xFullPathname */
	};
	return &bcVfs;
}


bc_vfs_file* bc_file_open(bc_vfs* pVfs, const char *fName,  const char* mode){
	return pVfs->xFopen(pVfs,fName, mode);
}

int bc_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset){
	return pFile->pMethods->xRead(pFile, buf, count, mode)
}

int bc_file_close(bc_vfs_file* pFile){
 	return pFile->pMethods->xClose(pFile);
 }

int bc_file_size(bc_vfs_file *pFile, uint64_t *pSize){
 	pFile->pMethods->xFileSize(pFile,pSize);
 }