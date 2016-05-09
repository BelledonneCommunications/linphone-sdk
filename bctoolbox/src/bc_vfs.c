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
 * Find the VFS in use within the VFS table.
 * @param  p        bc_vfs pointer to the first VFS
 * @param  zVfsName VFS name
 * @return          pointer to the VFS with the name zVfsName if found, NULL otherwise.
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
 * Closes file by closing the associated file stream.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile File handle pointer.
 * @param  pErrSvd  Pointer holding the errno value in case an error occurred.
 * @return       0 if successful, -1 otherwise.
 */
static int bcClose(sqlite3_file *p){

	/*The file descriptor is not dup'ed, and will be closed when the stream created by fdopen() is closed
	The fclose() function flushes the stream pointed to by fp (writing any buffered output data using fflush(3))
	 and closes the underlying file descriptor. */
	int ret;
	bc_vfs_file *pFile = (bc_vfs_file*) p;
	ret = close(pFile->fd);
	if (!ret){
		// close(pFile->fd);
		free(pFile);
		pFile = NULL;
		return 0;
	}
	else{
		bctoolbox_error("bcClose error %s", strerror(errno));
		free(pFile);
		return BCTBX_VFS_ERROR;
	}
}



/**
 * Repositions the open file offset given by the file descriptor fd from the file handle pFile
 * to the parameter offset, according to whence.
 * @param  pFile  File handle pointer.
 * @param  offset file offset where to position to
 * @param  whence Either SEEK_SET, SEEK_CUR,SEEK_END .
 * @return   offset bytes from the beginning of the file, BCTBX_VFS_ERROR otherwise
 */
static int bcSeek(bc_vfs_file *pFile, uint64_t offset, int whence){
	off_t ofst;
	if (pFile->fd > 0){
		ofst = lseek(pFile->fd, offset, whence);
		if( ofst < 0) {
			bctoolbox_error("bcSeek: Wrong offset %s"  ,strerror(errno));
			return BCTBX_VFS_ERROR;
		}
		return ofst;
	}
	bctoolbox_error(" bcSeek error %s", strerror(errno));
	return BCTBX_VFS_ERROR;
}
/**
 * Read count bytes from the open file given by pFile, starting at offset.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile  File handle pointer.
 * @param  buf    buffer to write the read bytes to.
 * @param  count  number of bytes to read
 * @param  offset file offset where to start reading
 * @return BCTBX_VFS_ERROR if erroneous read, number of bytes read (count) otherwise
 */
static int bcRead(sqlite3_file *p, void *buf, int count, sqlite_int64 offset){
	off_t ofst;                     /* Return value from lseek() */
	int nRead;                      /* Return value from read() */
	bc_vfs_file *pFile = (bc_vfs_file*) p;
	if (pFile){
		if((ofst = lseek(pFile->fd, offset, SEEK_SET)) == offset ){
			nRead = read(pFile->fd, buf, count);
			if( nRead > 0  ) return nRead;

			else if(nRead <= 0 ){
				if(errno){
					bctoolbox_error("bcRead error : %s  \r\n", strerror(errno));
					return BCTBX_VFS_ERROR;
				}
				return 0;
			}
		}
		bctoolbox_error("bcRead: error offset");
		return BCTBX_VFS_ERROR;
	}
	bctoolbox_error("bcRead: error bc_vfs_file not initialized");
	return BCTBX_VFS_ERROR;
}

/**
 * Writes directly to the open file given through the pFile argument.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  p       File handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @param  pErrSvd [description
 * @return         number of bytes written (can be 0), BCTBX_VFS_ERROR if an error occurred.
 */
static int bcWrite(sqlite3_file *p, const void *buf, int count, sqlite_int64 offset){
	off_t ofst;                     /* Return value from lseek() */
	size_t nWrite = 0;                 /* Return value from write() */
	bc_vfs_file *pFile = (bc_vfs_file*) p;
	if (p ){
		if((ofst = lseek(pFile->fd, offset, SEEK_SET)) == offset ){
			nWrite = write(pFile->fd, buf, count);
			if( nWrite > 0  ) return nWrite;
			
			else if(nWrite <= 0 ){
				if(errno){
					return BCTBX_VFS_ERROR;
				}
				return 0;
			}
		}
		bctoolbox_error("bcWrite: error offset");
		return BCTBX_VFS_ERROR;
	}
	bctoolbox_error("bcWrite: error bc_vfs_file not initialized");
	return BCTBX_VFS_ERROR;
}



/**
 * Returns the file size associated with the file handle pFile.
 * @param  pFile File handle pointer.
 * @return       BCTBX_VFS_ERROR if an error occurred, BCTBX_VFS_OK otherwise.
 */
static int bcFileSize(sqlite3_file *p, sqlite_int64 *pSize){

	int rc;                         /* Return code from fstat() call */
	struct stat sStat;              /* Output of fstat() call */
	bc_vfs_file *pFile = (bc_vfs_file*) p;
	rc = fstat(pFile->fd, &sStat);
	if( rc!=0 ) {
		bctoolbox_error("bcFileSize: Error file size  %s"  ,strerror(errno));
		return BCTBX_VFS_ERROR;
	}
	*pSize = sStat.st_size;
	return BCTBX_VFS_OK;

}


/**
 * From the file associated with the pFile handle using its stream member, reads
 * at most count-1 characters and stops after the first end of line character found
 * then stores it to the allocated buffer pointed by s.
 * @param  pFile File handle pointer.
 * @param  s     Buffer where to store the read line to.
 * @param  count Maximum number of characters to get to find an end of line.
 * @return       s if successful, NULL on error or when EOF has been reached while no character has been read.
 */
static char* bcFgets(bc_vfs_file* pFile, char* s, int count){
	if (pFile->file != NULL) return fgets(s, count, pFile->file);

	return NULL;
}

/**
 * Prints the characters in the s buffer to the stream associated with the pFile handle.
 * @param  pFile File handle pointer.
 * @param  s Buffer  holding the string to be printed out.
 * @return  number of bytes printed if successful, negative value otherwise.
 */
static int bcFprintf(bc_vfs_file* pFile, const char* s){
	if (pFile->file != NULL) return fprintf(pFile->file, s);
	bctoolbox_error("bcFprintf  error %s", strerror(errno));
	return BCTBX_VFS_ERROR;
}

/**
 * Gets a line of max_len length and stores it to the allocaed buffer s.
 * Reads at most max_len characters from the file descriptor associated with the argument pFile
 * and looks for an end of line character. Stores the line found 
 * into the buffer pointed by s.
 * Reads another max_len character while the end of line character is not found
 * and the end of file is not reached.
 * Modifies the open file offset using pFile->offset.
 *
 * @param  pFile   File handle pointer.
 * @param  s       Buffer where to store the line.
 * @param  max_len Maximum number of characters to read in one fetch.
 * @return         size of line read, 0 if empty
 */
static int bcGetLine(bc_vfs_file *pFile, char* s,  int max_len) {
	
	if (pFile->fd < 0) {

		return NULL;
	}
	
	int lineMaxLgth = max_len;
	char *pTmpLineBuf = (char *)calloc( lineMaxLgth, sizeof(char));

	char* pNextLine = NULL;
	
	if (pTmpLineBuf == NULL) {
		bctoolbox_error("bcGetLine : Error allocating memory for line buffer.");
		return BCTBX_VFS_ERROR;
	}
	
	int ret = bctbx_file_read(pFile, pTmpLineBuf, lineMaxLgth, pFile->offset);

//	read return 0 if EOF
	
	int sizeofline = 0;
	
	while (((pNextLine = strstr(pTmpLineBuf, "\n")) == NULL) && (ret > 0)) {
		if (ret > 0) {
			lineMaxLgth += lineMaxLgth;
			pTmpLineBuf = realloc(pTmpLineBuf, lineMaxLgth);
			if (pTmpLineBuf == NULL) {
				bctoolbox_error("bcGetLine: Error reallocating space for line buffer.");
				free(pTmpLineBuf);
				return  BCTBX_VFS_ERROR;
			}
			ret = bctbx_file_read(pFile, pTmpLineBuf, lineMaxLgth,pFile->offset);

		}
		
	}
	
	if (ret > 0){
	// offset to next beginning of line
		sizeofline = strcspn(pTmpLineBuf, "\n") +1;
		pFile->offset += sizeofline ;
		strncpy(s, pTmpLineBuf, sizeofline );
		
	}
	else if (ret <0){
		bctoolbox_error("bcGetLine error ");
	}
	else{
		bctoolbox_warning("bcGetLine : EOF reached");
	}
	
	free(pTmpLineBuf);
	return sizeofline;
}


/**
 * [set_flags description]
 * @param  mode [description]
 * @return      [description]
 */
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

	return oflags;
}

/**
 * [bcFopen description]
 * @param  pVfs    [description]
 * @param  fName   [description]
 * @param  mode    [description]
 * @return         [description]
 */
static  int bcFopen(sqlite3_vfs *pVfs, const char *fName, sqlite3_file *p, int flags, int *pOutFlags ){
	static const sqlite3_io_methods bcio = {
		1,
		bcClose,                    /* xClose */
		bcRead,                     /* xRead */
		bcWrite,                    /* xWrite */
		0,
		0,
		bcFileSize,                 /* xFileSize */
		0,
		0,
		0,
		0,
		0,
		0,
	};

	bc_vfs_file *pFile = (bc_vfs_file*) p;
	if (pFile == NULL || fName == NULL){
		return NULL ;
	}

	memset(pFile, 0, sizeof(bc_vfs_file));
	int oflags = 0;

	pFile->fd = open(fName, flags, S_IRUSR | S_IWUSR);
	if( pFile->fd < 0 ){
		return NULL;
	}
//	pFile->file = fdopen(pFile->fd, mode);
//	if( pFile->file == NULL ){
//		return NULL;
//	}
	if( pOutFlags ){
    	*pOutFlags = oflags;
  	}
	pFile->base.pMethods = &bcio;
	pFile->filename = (char*)fName;
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

/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_demovfs(), 0);
*/
sqlite3_vfs *sqlite3_bctbx_vfs(void){
  static sqlite3_vfs bctbx_vfs = {
    1,                            /* iVersion */
    sizeof(bc_vfs_file),             /* szOsFile */
    MAXPATHNAME,                  /* mxPathname */
    0,                            /* pNext */
    "bctbx_vfs",                       /* zName */
    0,                            /* pAppData */
    bcFopen,                     /* xOpen */
    0,                   /* xDelete */
    0,                   /* xAccess */
    0,             /* xFullPathname */
    0,                   /* xDlOpen */
    0,                  /* xDlError */
    0,                    /* xDlSym */
    0,                  /* xDlClose */
    0,               /* xRandomness */
    0,                    /* xSleep */
    0,              /* xCurrentTime */
  };
  return &bctbx_vfs;
}


/**
 * [bctbx_file_write description]
 * @param  pFile  File handle pointer.
 * @param  buf    [description]
 * @param  count  [description]
 * @param  offset [description]
 * @return        [description]
 */
int bctbx_file_write(bc_vfs_file* pFile, const void *buf, int count, uint64_t offset){
	int ret;
	if (pFile!=NULL) {
		ret = pFile->base.pMethods->xWrite(&pFile->base,buf, count, offset);
		if (ret < 0)
		{
			bctoolbox_error("bctbx_file_write error %s", strerror(errno));

			return BCTBX_VFS_ERROR;
		}
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

/**
 * [bctbx_file_open description]
 * @param  pVfs  Pointer to the vfs instance in use.
 * @param  fName [description]
 * @param  mode  [description]
 * @return       [description]
 */
bc_vfs_file* bctbx_file_open(sqlite3_vfs* pVfs, const char *fName,  const char* mode ){
	
	int ret;
	bc_vfs_file* pFile =  ( bc_vfs_file *)calloc(sizeof(bc_vfs_file),1);
	
	sqlite3_file* p_ret = &pFile->base;

	int flags = set_flags(mode);
	ret = pVfs->xOpen(pVfs,fName, p_ret,flags , NULL );
	if (pFile == NULL || pFile->fd < 0){
		bctoolbox_error("bctbx_file_open: Error open %s"  ,strerror(errno));
		return NULL;
	}
	sqlite_int64 *pSize = (sqlite_int64 *)malloc(sizeof(sqlite_int64));
	if (pSize != NULL){
		ret = bctbx_file_size(pFile, pSize);
		pFile->size = *pSize;
		free(pSize);
	}
	
	
	return pFile;
	
}

/**
 * [bctbx_file_read description]
 * @param  pFile  File handle pointer.
 * @param  buf    [description]
 * @param  count  [description]
 * @param  offset [description]
 * @return        [description]
 */
int bctbx_file_read(bc_vfs_file* pFile, void *buf, int count, uint64_t offset){
	int ret;
	if (pFile){
		//Are we trying to read after the EOF?
		if (offset >= pFile->size)
		{
			//nothing to read
			return 0;
		}

		ret = pFile->base.pMethods->xRead(&pFile->base, buf, count, offset);
		//check if error : in this case pErrSvd is initialized
		if(ret <= 0 && errno ){
			bctoolbox_error("bctbx_file_read: Error read %s"  ,strerror(errno));
			return BCTBX_VFS_ERROR;
		}
		return ret;
	}

	 return BCTBX_VFS_ERROR;
}

/**
 * [bctbx_file_close description]
 * @param  pFile File handle pointer.
 * @return      return value from the xClose VFS Close function.
 */
int bctbx_file_close(bc_vfs_file* pFile){
	int ret;

 	if (pFile){
 		ret = pFile->base.pMethods->xClose(&pFile->base);
 		if (ret != 0 && errno)
		{
			bctoolbox_error("bctbx_file_close: Error  %s"  ,strerror(errno));
		}
		return ret;
 	} 
 	return BCTBX_VFS_ERROR;
 }

/**
 * [bctbx_file_size description]
 * @param  pFile File handle pointer.
 * @return       [description]
 */
uint64_t bctbx_file_size(bc_vfs_file *pFile, sqlite_int64 *pSize){
 	return pFile->base.pMethods->xFileSize(&pFile->base, pSize);
 }

/**
 * [bctbx_file_printf description]
 * @param  pFile  File handle pointer.
 * @param  offset where to write in the file
 * @param  fmt    format argument, similar to that of printf
 * @return        [description]
 */
int bctbx_file_fprintf(bc_vfs_file* pFile, uint64_t offset, const char* fmt, ...){
	
	char* ret = NULL;;
	va_list args;
	int count = 0;
	va_start (args, fmt);
	ret = bctoolbox_strdup_vprintf(fmt, args);
	if(ret != NULL){
		va_end(args);
		count=strlen(ret);

		if (offset !=0) pFile->offset = offset;
		int r = bctbx_file_write(pFile, ret, count, pFile->offset);
		free(ret);
		if (r>0) pFile->offset += r;
		return r ;
	}
	return -1;
	
}

/**
 * [bctbx_file_seek description]
 * @param  pFile  [description]
 * @param  offset [description]
 * @param  whence [description]
 * @return        [description]
 */
int bctbx_file_seek(bc_vfs_file *pFile, uint64_t offset, int whence){
	if (pFile) return lseek(pFile->fd,offset,whence);

	return BCTBX_VFS_ERROR;
}

/**
 * [bctbx_file_get_nxtline description]
 * @param  pFile  File handle pointer.
 * @param  s      [description]
 * @param  maxlen [description]
 * @return        [description]
 */
int bctbx_file_get_nxtline(bc_vfs_file* pFile, char*s , int maxlen){
	if (pFile) return bcGetLine(pFile,s, maxlen);

	return BCTBX_VFS_ERROR;
}