/*
bc_vfs.c
Copyright (C) 2016 Belledonne Communications SARL

This program is bctoolbox_free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "bctoolbox/bc_vfs.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <sys/types.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>



/**
 * Assigns the VFS pointer in use pVfs to pToVfs.
 * @param  pVfs   Pointer to the vfs instance in use.
 * @param  pToVfs Pointer to the vfs pointer in use.
 * @return       BCTBX_VFS_ERROR if pVfs is NULL,  BCTBX_VFS_OK otherwise.
 */
int bctbx_vfs_register(bctbx_vfs* pVfs, bctbx_vfs** pToVfs){
	int ret = BCTBX_VFS_OK;
	if (pVfs != NULL) *pToVfs = pVfs;
	else{
			ret = BCTBX_VFS_ERROR;
	}
	return ret;
}


/**
 * Closes file by closing the associated file descriptor.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile 	bctbx_vfs_file File handle pointer.
 * @param  pErrSvd  Pointer holding the errno value in case an error occurred.
 * @return       	BCTBX_VFS_OK if successful, BCTBX_VFS_ERROR otherwise.
 */
static int bcClose(bctbx_vfs_file *pFile, int *pErrSvd){

	int ret;
	ret = close(pFile->fd);
	if (!ret){
		ret = BCTBX_VFS_OK;
	}
	else{
		pErrSvd = (int*)bctoolbox_malloc((sizeof(int)));
		if (pErrSvd) *pErrSvd = errno;
		bctoolbox_error("bcClose error %s", strerror(errno));
		ret = BCTBX_VFS_ERROR;
	}
	return ret;
}



/**
 * Repositions the open file offset given by the file descriptor fd from the file handle pFile
 * to the parameter offset, according to whence.
 * @param  pFile  File handle pointer.
 * @param  offset file offset where to position to
 * @param  whence Either SEEK_SET, SEEK_CUR,SEEK_END .
 * @return   offset bytes from the beginning of the file, BCTBX_VFS_ERROR otherwise
 */
static int bcSeek(bctbx_vfs_file *pFile, uint64_t offset, int whence){
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
static int bcRead(bctbx_vfs_file *pFile, void *buf, int count, uint64_t offset, int* pErrSvd){
	off_t ofst;                     /* Return value from lseek() */
	int nRead;                      /* Return value from read() */
	if (pFile){
		if((ofst = pFile->pMethods->pFuncSeek(pFile, offset,SEEK_SET)) == offset ){
			nRead = read(pFile->fd, buf, count);
			/* Error while reading */
			if(nRead < 0 ){
				if(errno){
					pErrSvd = (int*)bctoolbox_malloc((sizeof(int)));
					bctoolbox_error("bcRead error \r\n");
					if (pErrSvd) *pErrSvd = errno;
					return BCTBX_VFS_ERROR;
				}
			}
			else if (nRead == count){
				return nRead;
			}
			/*read less than expected */
			else{
				  /* Buffer too big for what's been read : these parts of the buffer 
				  must be zero-filled */
				memset(&((char*)buf)[nRead], 0, count-nRead);
				return nRead;
			}
			
		}
		else{
	  		bctoolbox_error("bcRead: error offset");
			return BCTBX_VFS_ERROR;
		}	
	}
	bctoolbox_error("bcRead: error bctbx_vfs_file not initialized");
	return BCTBX_VFS_ERROR;
}

/**
 * Writes directly to the open file given through the pFile argument.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  p       bctbx_vfs_file File handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @param  pErrSvd [description
 * @return         number of bytes written (can be 0), BCTBX_VFS_ERROR if an error occurred.
 */
static int bcWrite(bctbx_vfs_file *p, const void *buf, int count, uint64_t offset , int* pErrSvd){
	off_t ofst;                     /* Return value from lseek() */
	size_t nWrite = 0;                 /* Return value from write() */

	if (p ){
		if((ofst = p->pMethods->pFuncSeek(p, offset,SEEK_SET)) == offset ){
			nWrite = write(p->fd, buf, count);
			if( nWrite > 0  ) return nWrite;
			
			else if(nWrite <= 0 ){
				if(errno){
					pErrSvd = (int*)bctoolbox_malloc((sizeof(int)));
					if (pErrSvd) *pErrSvd = errno;
					return BCTBX_VFS_ERROR;
				}
				return 0;
			}
		}
		bctoolbox_error("bcWrite: error offset");
		return BCTBX_VFS_ERROR;
	}
	bctoolbox_error("bcWrite: error bctbx_vfs_file not initialized");
	return BCTBX_VFS_ERROR;
}



/**
 * Returns the file size associated with the file handle pFile.
 * @param  pFile File handle pointer.
 * @return       BCTBX_VFS_ERROR if an error occurred, file size otherwise (can be 0).
 */
static int bcFileSize(bctbx_vfs_file *pFile){

	int rc;                         /* Return code from fstat() call */
	struct stat sStat;              /* Output of fstat() call */
	
	rc = fstat(pFile->fd, &sStat);
	if( rc!=0 ) {
		bctoolbox_error("bcFileSize: Error file size  %s"  ,strerror(errno));
		return BCTBX_VFS_ERROR;
	}
	return sStat.st_size;

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
static int bcGetLine(bctbx_vfs_file *pFile, char* s,  int max_len) {
	int ret, sizeofline, isEof ;
	int lineMaxLgth;
	char *pTmpLineBuf ;
	char* pNextLine ;

	if (pFile->fd < 0) {

		return BCTBX_VFS_ERROR;
	}

	pNextLine = NULL;
	lineMaxLgth = max_len;
	pTmpLineBuf = (char *)calloc( lineMaxLgth, sizeof(char));

	sizeofline = 0;
	isEof = 0;
	
	if (pTmpLineBuf == NULL) {
		bctoolbox_error("bcGetLine : Error allocating memory for line buffer.");
		return BCTBX_VFS_ERROR;
	}

	/* Read returns 0 if end of file is found */
	ret = bctbx_file_read(pFile, pTmpLineBuf, lineMaxLgth, pFile->offset);

	
	while ((((pNextLine = strstr(pTmpLineBuf, "\n")) == NULL)  && (pNextLine = strstr(pTmpLineBuf, "\r")) == NULL)&& (ret > 0) && (!isEof)) {
		/*  did not find end of line in lineMaxLgth, try to get another lineMaxLgth characters */
		if (ret % lineMaxLgth == 0) {
			lineMaxLgth += lineMaxLgth;
			pTmpLineBuf = bctoolbox_realloc(pTmpLineBuf, lineMaxLgth);
			if (pTmpLineBuf == NULL) {
				bctoolbox_error("bcGetLine: Error reallocating space for line buffer.");
				bctoolbox_free(pTmpLineBuf);
				return  BCTBX_VFS_ERROR;
			}
		}
		else {
			/* check if EOF has been reached */
			if ( (pFile->offset + ret) == pFile->size){
					isEof = 1;
				}
		}
			ret = bctbx_file_read(pFile, pTmpLineBuf, lineMaxLgth,pFile->offset);

		
		
	}
	
	if (ret > 0){
	/* Got a line! */
	/* offset to next beginning of line*/
		if(strstr(pTmpLineBuf, "\r")) sizeofline = strcspn(pTmpLineBuf, "\r") +1;
		if(strstr(pTmpLineBuf, "\n")) sizeofline = strcspn(pTmpLineBuf, "\n") +1;
		pFile->offset += sizeofline ;
		strncpy(s, pTmpLineBuf, sizeofline );
		
	}
	else if (ret <0){
		bctoolbox_error("bcGetLine error ");
	}
	else{
		bctoolbox_warning("bcGetLine : EOF reached");
	}
	
	bctoolbox_free(pTmpLineBuf);
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

static const bctbx_io_methods bcio = {
		bcClose,                    /* pFuncClose */
		bcRead,                     /* pFuncRead */
		bcWrite,                    /* pFuncWrite */
		bcFileSize,                 /* pFuncFileSize */
		bcGetLine,
		bcSeek,
};

const bctbx_io_methods* get_bcio(void){
	return &bcio;
}
/**
 * [bcFopen description]
 * @param  pVfs    [description]
 * @param  fName   [description]
 * @param  mode    [description]
 * @param  pErrSvd [description]
 * @return         [description]
 */
static  int bcOpen(bctbx_vfs *pVfs, bctbx_vfs_file *pFile, const char *fName, const int openFlags, int* pErrSvd){

	
	if (pFile == NULL || fName == NULL){
		return BCTBX_VFS_ERROR ;
	}

	
	pFile->fd = open(fName, openFlags, S_IRUSR | S_IWUSR);
	if( pFile->fd == -1 ){
		pErrSvd = (int*)bctoolbox_malloc((sizeof(int)));
		if (pErrSvd) *pErrSvd = errno;
		return BCTBX_VFS_ERROR;
	}
	
	pFile->pMethods = &bcio;
	pFile->size = pFile->pMethods->pFuncFileSize(pFile);
	pFile->filename = (char*)fName;
	return BCTBX_VFS_OK;
}




/*
 ** This function returns a pointer to the VFS implemented in this file.
 **
 */
bctbx_vfs *bc_create_vfs(void){
	static bctbx_vfs bcVfs = {
		0,                            /* pNext */
		"bctbx_vfs",                     /* vfsName */
		bcOpen,						/*xOpen */

	};
	return &bcVfs;
}

/**
 * [bctbx_file_write description]
 * @param  pFile  File handle pointer.
 * @param  buf    [description]
 * @param  count  [description]
 * @param  offset [description]
 * @return        [description]
 */
int bctbx_file_write(bctbx_vfs_file* pFile, const void *buf, int count, uint64_t offset){
	int* pErrSvd = NULL;
	int ret;
	if (pFile!=NULL) {
		ret = pFile->pMethods->pFuncWrite(pFile,buf, count, offset, pErrSvd);
		if (pErrSvd)
		{
			bctoolbox_error("bctbx_file_write error %s", strerror(*pErrSvd));
			bctoolbox_free(pErrSvd);	
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
bctbx_vfs_file* bctbx_file_create_and_open(bctbx_vfs* pVfs, const char *fName,  const char* mode){
	int ret;

	bctbx_vfs_file* p_ret = (bctbx_vfs_file*)calloc(sizeof(bctbx_vfs_file),1);
	int oflags = 0;
	oflags = set_flags(mode);
	if(p_ret){
		ret = bctbx_file_open(pVfs,p_ret,fName, oflags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}
	
	return NULL;
}

/**
 * [bctbx_file_open description]
 * @param  pVfs  Pointer to the vfs instance in use.
 * @param  fName [description]
 * @param  mode  [description]
 * @return       [description]
 */
int bctbx_file_open(bctbx_vfs* pVfs, bctbx_vfs_file* pFile, const char *fName,  const int oflags){
	int* pErrSvd = NULL;
	int ret;
	if (pVfs && pFile ){

		ret = pVfs->pFuncFopen(pVfs,pFile,fName, oflags, pErrSvd);
		if (ret == BCTBX_VFS_ERROR && pErrSvd){
			bctoolbox_error("bctbx_file_open: Error open %s" , strerror(*pErrSvd));
			bctoolbox_free(pErrSvd);
			return BCTBX_VFS_ERROR;
		}
		ret = bctbx_file_size(pFile);
		
		return BCTBX_VFS_OK;
	}
	return BCTBX_VFS_ERROR;
}
/**
 * Attempts to read count bytes from the open file given by pFile, at the position starting at offset 
 * in the file and and puts them in the buffer pointed by buf.
 * @param  pFile  bctbx_vfs_file File handle pointer.
 * @param  buf    Buffer holding the read bytes.
 * @param  count  Number of bytes to read. 
 * @param  offset Where to start reading in the file (in bytes).
 * @return        [description]
 */
int bctbx_file_read(bctbx_vfs_file* pFile, void *buf, int count, uint64_t offset){
	int* pErrSvd = NULL;
	int ret;
	if (pFile){
		/*Are we trying to read after the EOF? Then there is nothing to read.*/
		if (offset >= pFile->size && pFile->size!= 0)
		{
			return 0;
		}
		ret = pFile->pMethods->pFuncRead(pFile, buf, count, offset, pErrSvd);
		/*check if error : in this case pErrSvd is initialized*/
		if(ret <= 0 && pErrSvd!=NULL){
			bctoolbox_error("bctbx_file_read: Error read %s"  ,strerror(*pErrSvd));
			bctoolbox_free(pErrSvd);
			return BCTBX_VFS_ERROR;
		}
		return ret;
	}
	 return BCTBX_VFS_ERROR;
}

/**
 * [bctbx_file_close description]
 * @param  pFile File handle pointer.
 * @return      return value from the pFuncClose VFS Close function.
 */
int bctbx_file_close(bctbx_vfs_file* pFile){
	int ret;
	int* pErrSvd = NULL;
 	if (pFile){
 		ret = pFile->pMethods->pFuncClose(pFile, pErrSvd);
 		if (ret != 0 && pErrSvd)
		{
			bctoolbox_error("bctbx_file_close: Error  %s"  ,strerror(*pErrSvd));
			bctoolbox_free(pErrSvd);
		}
		return ret;
 	} 
 	return BCTBX_VFS_ERROR;
 }


/**
 * [bctbx_file_close description]
 * @param  pFile File handle pointer.
 * @return      return value from the pFuncClose VFS Close function.
 */
int bctbx_file_close_and_free(bctbx_vfs_file* pFile){
	int ret;
	ret = bctbx_file_close(pFile);
	free(pFile);
	return ret;
}

/**
 * Returns the file size.
 * @param  pFile  bctbx_vfs_file File handle pointer.
 * @return       -1 if an error occured, file size otherwise. 
 */
uint64_t bctbx_file_size(bctbx_vfs_file *pFile ){
	if (pFile) return pFile->pMethods->pFuncFileSize(pFile);
 	return BCTBX_VFS_ERROR;
 }

/**
 * [bctbx_file_printf description]
 * @param  pFile  File handle pointer.
 * @param  offset where to write in the file
 * @param  fmt    format argument, similar to that of printf
 * @return        [description]
 */
int bctbx_file_fprintf(bctbx_vfs_file* pFile, uint64_t offset, const char* fmt, ...){
	
	char* ret = NULL;
	va_list args;
	int count , r ;
	count = 0;

	va_start (args, fmt);
	ret = bctoolbox_strdup_vprintf(fmt, args);
	if(ret != NULL){
		va_end(args);
		count = strlen(ret);

		if (offset !=0) pFile->offset = offset;
		r = bctbx_file_write(pFile, ret, count, pFile->offset);
		bctoolbox_free(ret);
		if (r>0) pFile->offset += r;
		return r ;
	}
	return BCTBX_VFS_ERROR;
	
}

/**
 * [bctbx_file_seek description]
 * @param  pFile  [description]
 * @param  offset [description]
 * @param  whence [description]
 * @return        [description]
 */
int bctbx_file_seek(bctbx_vfs_file *pFile, uint64_t offset, int whence){
	if (pFile) return pFile->pMethods->pFuncSeek(pFile,offset,whence);

	return BCTBX_VFS_ERROR;
}

/**
 * [bctbx_file_get_nxtline description]
 * @param  pFile  File handle pointer.
 * @param  s      [description]
 * @param  maxlen [description]
 * @return        [description]
 */
int bctbx_file_get_nxtline(bctbx_vfs_file* pFile, char*s , int maxlen){
	if (pFile) return pFile->pMethods->pFuncGetLineFromFd(pFile,s, maxlen);

	return BCTBX_VFS_ERROR;
}
