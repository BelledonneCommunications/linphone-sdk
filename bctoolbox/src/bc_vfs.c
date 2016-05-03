/***************************************************************************
 *            bc_vfs.c
 *
 *  Mon May 02 11:13:44 2016
 *  Copyright  2016  Simon Morlat
 *  Email simon.morlat@linphone.org
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "bctoolbox/bc_vfs.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

/**
 * [bc_vfs_find description]
 * @param  p        [description]
 * @param  zVfsName [description]
 * @return          [description]
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
 * [bc_vfs_register description]
 * @param  pVfs   Pointer to the vfs instance in use.
 * @param  pToVfs [description]
 * @return        [description]
 */
int bc_vfs_register(bc_vfs* pVfs, bc_vfs** pToVfs){
	int ret = 1;
	if (pVfs != NULL) *pToVfs = pVfs;
	else{
			ret = 0;
	}
	return ret;
}

/**
 * [bc_vfs_init description]
 * @param  pVfs    Pointer to the vfs instance in use.
 * @param  pToVfs  [description]
 * @param  pToFile [description]
 * @return         [description]
 */
bool bc_vfs_init(bc_vfs* pVfs, bc_vfs* pToVfs, bc_vfs_file* pToFile){
	bool ret = 1;
	bc_vfs_file *pFile = (bc_vfs_file*)calloc(sizeof(bc_vfs_file),1);
	if (pFile != NULL){
		pToFile = pFile;
	}
	else{ 
		ret = 0;
	}	
	if (pToVfs != NULL) pToVfs = pVfs;
	else{
		ret = 0;
	}
	return ret;
}



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
static int bcWrite(bc_vfs_file *p, const void *buf, int count, uint64_t offset ){
	off_t ofst;                     /* Return value from lseek() */
	size_t nWrite;                  /* Return value from write() */
	if (p->fd > 0){
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
	else{
		return BC_VFS_IOERR_WRITE;
	}
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

static char *bcGetLine(bc_vfs_file *pFile, char* s,  int max_len) {
	
	if (pFile->fd < 0) {

		return NULL;
	}
	
	int lineMaxLgth = max_len;
	char *pTmpLineBuf = (char *)malloc(sizeof(char) * lineMaxLgth);
	char* pLineBuf = NULL;
	char* pNextLine = NULL;
	
	if (pTmpLineBuf == NULL) {
		printf("Error allocating memory for line buffer.");
		return NULL;
	}
	
	int ret = bc_file_read(pFile, pTmpLineBuf, lineMaxLgth, pFile->offset);
//	read return 0 if EOF
	
	int sizeofline = 0;
	

	while (((pNextLine = strstr(pTmpLineBuf, "\n")) == NULL)) {
		if (ret == 0) {
			lineMaxLgth += lineMaxLgth;
			pTmpLineBuf = realloc(pTmpLineBuf, lineMaxLgth);
			if (pTmpLineBuf == NULL) {
				printf("Error reallocating space for line buffer.");
				return  NULL;
			}
			ret = bc_file_read(pFile, pTmpLineBuf, lineMaxLgth,pFile->offset);

		}
	}
	// offset to next beginning of line
	pNextLine = pNextLine +1;
	pFile->offset = (pNextLine - pTmpLineBuf)/sizeof(char);
	sizeofline = strcspn(pTmpLineBuf, "\n");
//	pLineBuf = (char *)malloc(sizeof(char) * sizeofline);
	strncpy(s, pTmpLineBuf, sizeofline );
//	free(pTmpLineBuf);
//	s = pLineBuf;
	return pNextLine;
}


static int set_flags(const char* mode){
	int flags;
	int oflags = 0;                 /* flags to pass to open() call */
	if (strcmp(mode, "r") == 0) {
  		flags =  O_RDONLY;
	} 
	if ((strcmp(mode, "r+") == 0) || (strcmp(mode, "w+") == 0)) {
		flags =  O_RDWR;

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
		bcFileSize,                 /* xFileSize */
		bcGetLine,
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
	
	pFile->pMethods = &bcio;
	return pFile;
}




/*
 ** This function returns a pointer to the VFS implemented in this file.
 **
 */
bc_vfs *bc_create_vfs(void){
	static bc_vfs bcVfs = {
		0,                            /* pNext */
		"bc_vfs",                     /* vfsName */
		bcFopen,						/*xFopen */

	};
	return &bcVfs;
}

int bc_file_write(bc_vfs_file* pFile, const void *buf, int count, uint64_t offset){
	if (pFile!=NULL) {
		return pFile->pMethods->xWrite(pFile,buf, count, offset);
	}
	return -1;
}

bc_vfs_file* bc_file_open(bc_vfs* pVfs, const char *fName,  const char* mode){
	return pVfs->xFopen(pVfs,fName, mode);
}

int bc_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset){
	return pFile->pMethods->xRead(pFile, buf, count, offset);
}

int bc_file_close(bc_vfs_file* pFile){
 	return pFile->pMethods->xClose(pFile);
 }

int bc_file_size(bc_vfs_file *pFile, uint64_t *pSize){
 	return pFile->pMethods->xFileSize(pFile,pSize);
 }


int bc_file_printf(bc_vfs_file* pFile, uint64_t offset, const char* fmt, ...){
	
	char* ret;
	va_list args;
	int count = 0;
	va_start (args, fmt);
	ret = bctoolbox_strdup_vprintf(fmt, args);
	va_end(args);
	count = sizeof(ret);
	bc_file_write(pFile, ret, count, offset);
	return sizeof(ret);
	
}

char * bc_file_get_nxtline(bc_vfs_file* pFile, char*s , int maxlen){
	if (pFile){
		return pFile->pMethods->xFgets(pFile,s, maxlen);
	}
	return NULL;
}