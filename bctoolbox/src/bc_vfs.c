/*
bc_vfs.c
Copyright (C) 2016 Belledonne Communications SARL

This program is free software; you can redistribute it and/or
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
#include <stdarg.h>
#include <errno.h>

static bctbx_vfs_t *pDefaultVfs;

/**
 * Closes file by closing the associated file descriptor.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile 	bctbx_vfs_file_t File handle pointer.
 * @return       	BCTBX_VFS_OK if successful, BCTBX_VFS_ERROR otherwise.
 */
static int bcClose(bctbx_vfs_file_t *pFile){

	int ret;
	ret = close(pFile->fd);
	if (!ret){
		ret = BCTBX_VFS_OK;
	}
	else{

		ret = -errno ;
	}
	return ret;
}



/**
 * Repositions the open file offset given by the file descriptor fd from the file handle pFile
 * to the parameter offset, according to whence.
 * @param  pFile  File handle pointer.
 * @param  offset file offset where to set the position to
 * @param  whence Either SEEK_SET, SEEK_CUR,SEEK_END .
 * @return   offset bytes from the beginning of the file, BCTBX_VFS_ERROR otherwise
 */
static int bcSeek(bctbx_vfs_file_t *pFile, uint64_t offset, int whence){
	off_t ofst;
	if (pFile){
		ofst = lseek(pFile->fd, offset, whence);
		if( ofst < 0) {
			
			return -errno;
		}
		return ofst;
	}
	bctbx_error(" bcSeek: File  error ");
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
 * @return -errno if erroneous read, number of bytes read (count) on success, 
 *                if the error was something else BCTBX_VFS_ERROR otherwise
 */
static int bcRead(bctbx_vfs_file_t *pFile, void *buf, int count, uint64_t offset){
	int nRead;                      /* Return value from read() */
	if (pFile){
		if(bctbx_file_seek(pFile, offset,SEEK_SET) == BCTBX_VFS_OK ){
			nRead = read(pFile->fd, buf, count);
			/* Error while reading */
			if(nRead < 0 ){
				if(errno){
					bctbx_error("bcRead error \r\n");
					return -errno;
				}
			}
			return nRead;
			
		}
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Writes directly to the open file given through the pFile argument.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  p       bctbx_vfs_file_t File handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @return         number of bytes written (can be 0), negative value errno if an error occurred.
 */
static int bcWrite(bctbx_vfs_file_t *p, const void *buf, int count, uint64_t offset ){
	size_t nWrite = 0;                 /* Return value from write() */

	if (p ){
		if((bctbx_file_seek(p, offset,SEEK_SET)) == BCTBX_VFS_OK ){
			nWrite = write(p->fd, buf, count);
			if( nWrite > 0  ) return nWrite;
			
			else if(nWrite <= 0 ){
				if(errno){
					return -errno;
				}
				return 0;
			}
		}
	}
	return BCTBX_VFS_ERROR;
}



/**
 * Returns the file size associated with the file handle pFile.
 * @param  pFile File handle pointer.
 * @return       -errno if an error occurred, file size otherwise (can be 0).
 */
static int bcFileSize(bctbx_vfs_file_t *pFile){

	int rc;                         /* Return code from fstat() call */
	struct stat sStat;              /* Output of fstat() call */
	
	rc = fstat(pFile->fd, &sStat);
	if( rc!=0 ) {
		return -errno;
	}
	return sStat.st_size;

}



/**
 * Gets a line of max_len length and stores it to the allocaed buffer s.
 * Reads at most max_len characters from the file descriptor associated with the argument pFile
 * and looks for an end of line character (\r or \n). Stores the line found
 * into the buffer pointed by s.
 * Modifies the open file offset using pFile->offset.
 *
 * @param  pFile   File handle pointer.
 * @param  s       Buffer where to store the line.
 * @param  max_len Maximum number of characters to read in one fetch.
 * @return         size of line read, 0 if empty
 */
static int bcGetLine(bctbx_vfs_file_t *pFile, char* s,  int max_len) {
	int ret, sizeofline, isEof ;
	char* pNextLine ;
	
	if (pFile->fd == -1) {
		return BCTBX_VFS_ERROR;
	}
	if (s == NULL || max_len<1) {
		return BCTBX_VFS_ERROR;
	}
	
	pNextLine = NULL;
	
	sizeofline = 0;
	isEof = 0;
	
	s[max_len-1] = '\0';
	
	/* Read returns 0 if end of file is found */
	ret = bctbx_file_read(pFile, s, max_len-1, pFile->offset);
	if (ret > 0){
		pNextLine = strchr(s, '\r');
		if (pNextLine == NULL) pNextLine = strchr(s, '\n');
		if (pNextLine)
		{
			/* Got a line! */
			*pNextLine = '\0';
			sizeofline = pNextLine - s + 1;
			if (pNextLine[1] == '\n') sizeofline += 1; /*take into account the \r\n" case*/
			
			/* offset to next beginning of line*/
			pFile->offset += sizeofline ;
		}else{
			/*did not find end of line char, is EOF?*/
			sizeofline = ret;
			pFile->offset += sizeofline ;
			s[ret] = '\0';
		}
	}else if (ret < 0){
		bctbx_error("bcGetLine error ");
	}
	return sizeofline;
}
/**
 * Create flags (int) from mode(char*).
 * @param  mode  Can be r, r+, w+, w
 * @return 		 returns flags (integer).	 
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

/*
 Returns the bctbx_io_methods address.
 */
const bctbx_io_methods* get_bcio(void){
	return &bcio;
}
/**
 * Opens the file with filename fName, associate it to the file handle pointed 
 * by pFile, sets the methods bctbx_io_methods to the bcio structure
 * and initializes the file size.
 * Sets the error in pErrSvd if an error occurred while opening the file fName.
 * @param  pVfs    		Pointer to  bctx_vfs  VFS.
 * @param  fName   		Absolute path filename.
 * @param  openFlags    Flags to use when opening the file.
 * @return         		BCTBX_VFS_ERROR if an error occurs, BCTBX_VFS_OK otherwise.
 */
static  int bcOpen(bctbx_vfs_t *pVfs, bctbx_vfs_file_t *pFile, const char *fName, const int openFlags){
	
	if (pFile == NULL || fName == NULL){
		return BCTBX_VFS_ERROR ;
	}

	pFile->fd = open(fName, openFlags, S_IRUSR | S_IWUSR);
	if( pFile->fd == -1 ){
		return -errno;
	}
	
	pFile->pMethods = &bcio;
	pFile->size = pFile->pMethods->pFuncFileSize(pFile);
	return BCTBX_VFS_OK;
}



bctbx_vfs_t *bc_create_vfs(void){
	static bctbx_vfs_t bcVfs = {
		"bctbx_vfs_t",                  /* vfsName */
		bcOpen,						/*xOpen */

	};
	return &bcVfs;
}



int bctbx_file_write(bctbx_vfs_file_t* pFile, const void *buf, int count, uint64_t offset){
	int ret;
	if (pFile!=NULL) {
		ret = pFile->pMethods->pFuncWrite(pFile,buf, count, offset);
		if (ret == BCTBX_VFS_ERROR)
		{
			bctbx_error("bctbx_file_write file error ");
			return BCTBX_VFS_ERROR;

		}
		else if (ret < 0)
		{
			bctbx_error("bctbx_file_write error %s", strerror(-(ret)));
			return BCTBX_VFS_ERROR;
		}
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

static int file_open(bctbx_vfs_t* pVfs, bctbx_vfs_file_t* pFile, const char *fName,  const int oflags){
	int ret = BCTBX_VFS_ERROR;
	if (pVfs && pFile ){

		ret = pVfs->pFuncFopen(pVfs,pFile,fName, oflags);
		if (ret == BCTBX_VFS_ERROR){
			bctbx_error("bctbx_file_open: Error file handle " );
		}
		else if (ret < 0 ){
			bctbx_error("bctbx_file_open: Error open %s" , strerror(-(ret)));
			
		}
		ret = bctbx_file_size(pFile);
		
		return BCTBX_VFS_OK;
	}
	return BCTBX_VFS_ERROR;
}

bctbx_vfs_file_t* bctbx_file_open(bctbx_vfs_t* pVfs, const char *fName,  const char* mode){
	int ret;

	bctbx_vfs_file_t* p_ret = (bctbx_vfs_file_t*)bctbx_malloc(sizeof(bctbx_vfs_file_t));
	int oflags = 0;
	oflags = set_flags(mode);
	if(p_ret){
		memset(p_ret, 0, sizeof(bctbx_vfs_file_t));
		ret = file_open(pVfs,p_ret,fName, oflags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}
	
	return NULL;
}


bctbx_vfs_file_t* bctbx_file_open2(bctbx_vfs_t* pVfs, const char *fName,  const int openFlags ){
	int ret;
	
	bctbx_vfs_file_t* p_ret = (bctbx_vfs_file_t*)bctbx_malloc(sizeof(bctbx_vfs_file_t));

	if(p_ret){
		memset(p_ret, 0, sizeof(bctbx_vfs_file_t));
		ret = file_open(pVfs,p_ret,fName, openFlags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}
	
	return NULL;
}



int bctbx_file_read(bctbx_vfs_file_t* pFile, void *buf, int count, uint64_t offset){
	int ret;
	if (pFile){
		ret = pFile->pMethods->pFuncRead(pFile, buf, count, offset);
		/*check if error : in this case pErrSvd is initialized*/
		if (ret == BCTBX_VFS_ERROR){
			bctbx_error("bctbx_file_read: error bctbx_vfs_file_t ");
			return BCTBX_VFS_ERROR;
		}
		else if(ret < 0){
			bctbx_error("bctbx_file_read: Error read %s" ,strerror(-(ret)));
			return BCTBX_VFS_ERROR;
		}
		return ret;
	}
	 return BCTBX_VFS_ERROR;
}


int bctbx_file_close(bctbx_vfs_file_t* pFile){
	int ret = BCTBX_VFS_ERROR;
 	if (pFile){
 		ret = pFile->pMethods->pFuncClose(pFile);
 		if (ret != 0 )
		{
			bctbx_error("bctbx_file_close: Error  %s freeing file handle anyway"  ,strerror(-(ret)));
		}
 	}
	bctbx_free(pFile);
 	return ret;
 }


uint64_t bctbx_file_size(bctbx_vfs_file_t *pFile ){
	int ret; 
	ret = BCTBX_VFS_ERROR;
	if (pFile){
		ret = pFile->pMethods->pFuncFileSize(pFile);
		if (ret < 0) bctbx_error("bctbx_file_size: Error file size  %s"  ,strerror(-(ret)));
	} 
 	return ret ;
 }


int bctbx_file_fprintf(bctbx_vfs_file_t* pFile, uint64_t offset, const char* fmt, ...){
	
	char* ret = NULL;
	va_list args;
	int count , r ;
	count = 0;

	va_start (args, fmt);
	ret = bctbx_strdup_vprintf(fmt, args);
	if(ret != NULL){
		va_end(args);
		count = strlen(ret);

		if (offset !=0) pFile->offset = offset;
		r = bctbx_file_write(pFile, ret, count, pFile->offset);
		bctbx_free(ret);
		if (r>0) pFile->offset += r;
		return r ;
	}
	return BCTBX_VFS_ERROR;
	
}


int bctbx_file_seek(bctbx_vfs_file_t *pFile, uint64_t offset, int whence){
	int ret = BCTBX_VFS_ERROR ;

	if (pFile){
		ret = pFile->pMethods->pFuncSeek(pFile,offset,whence);
		if(ret < 0){
			bctbx_error("bctbx_file_seek: Wrong offset %s"  ,strerror(-(ret)));
		}
		else if (ret == offset){
			ret = BCTBX_VFS_OK;
		}
	} 
	return ret;
}

int bctbx_file_get_nxtline(bctbx_vfs_file_t* pFile, char*s , int maxlen){
	if (pFile) return pFile->pMethods->pFuncGetLineFromFd(pFile,s, maxlen);

	return BCTBX_VFS_ERROR;
}


void bctbx_vfs_set_default(bctbx_vfs_t *my_vfs){
	if (my_vfs == NULL){
		pDefaultVfs = bc_create_vfs();
	}
	else{
		pDefaultVfs = my_vfs;
	}
}


bctbx_vfs_t* bctbx_vfs_get_default(void){
	return pDefaultVfs;
	
}