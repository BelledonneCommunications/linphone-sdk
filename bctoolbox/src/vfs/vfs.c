/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/vfs.h"
#include "bctoolbox/vfs_standard.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>


/* Pointer to default VFS initialized to standard VFS implemented here.*/
static bctbx_vfs_t *pDefaultVfs = &bcStandardVfs; /* bcStandardVfs is defined int vfs_standard.h*/

/**
 * Create flags (int) from mode(char*).
 * @param mode Can be r, r+, w+, w
 * @return flags (integer).	 
 */
static int set_flags(const char* mode) {
	int flags = 0 ;                 /* flags to pass to open() call */
	if (strcmp(mode, "r") == 0) {
  		flags =  O_RDONLY;
	} else if ((strcmp(mode, "r+") == 0) || (strcmp(mode, "w+") == 0)) {
		flags =  O_RDWR;
	} else if(strcmp(mode, "w") == 0) {
  		flags =  O_WRONLY;
	}
	return flags | O_CREAT;
}

ssize_t bctbx_file_write(bctbx_vfs_file_t* pFile, const void *buf, size_t count, off_t offset) {
	ssize_t ret;

	if (pFile != NULL) {
		ret = pFile->pMethods->pFuncWrite(pFile, buf, count, offset);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_write file error");
			return BCTBX_VFS_ERROR;
		} else if (ret < 0) {
			bctbx_error("bctbx_file_write error %s", strerror(-(ret)));
			return BCTBX_VFS_ERROR;
		}
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

static int file_open(bctbx_vfs_t* pVfs, bctbx_vfs_file_t* pFile, const char *fName, const int oflags) {
	int ret = BCTBX_VFS_ERROR;
	if (pVfs && pFile ) {
		ret = pVfs->pFuncOpen(pVfs, pFile, fName, oflags);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_open: Error file handle");
		} else if (ret < 0 ) {
			bctbx_error("bctbx_file_open: Error open %s", strerror(-(ret)));
			ret = BCTBX_VFS_ERROR;
		}
	}
	return ret;
}

bctbx_vfs_file_t* bctbx_file_open(bctbx_vfs_t *pVfs, const char *fName, const char *mode) {
	int ret;
	bctbx_vfs_file_t* p_ret = (bctbx_vfs_file_t*)bctbx_malloc(sizeof(bctbx_vfs_file_t));
	int oflags = set_flags(mode);
	if (p_ret) {
		memset(p_ret, 0, sizeof(bctbx_vfs_file_t));
		ret = file_open(pVfs, p_ret, fName, oflags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}

	if (p_ret) bctbx_free(p_ret);
	return NULL;
}

bctbx_vfs_file_t* bctbx_file_open2(bctbx_vfs_t *pVfs, const char *fName, const int openFlags) {
	int ret;
	bctbx_vfs_file_t *p_ret = (bctbx_vfs_file_t *)bctbx_malloc(sizeof(bctbx_vfs_file_t));

	if (p_ret) {
		memset(p_ret, 0, sizeof(bctbx_vfs_file_t));
		ret = file_open(pVfs, p_ret, fName, openFlags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}

	if (p_ret) bctbx_free(p_ret);
	return NULL;
}

ssize_t bctbx_file_read(bctbx_vfs_file_t *pFile, void *buf, size_t count, off_t offset) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile) {
		ret = pFile->pMethods->pFuncRead(pFile, buf, count, offset);
		/*check if error : in this case pErrSvd is initialized*/
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_read: error bctbx_vfs_file_t");
		}
		else if (ret < 0) {
			bctbx_error("bctbx_file_read: Error read %s", strerror(-(ret)));
			ret = BCTBX_VFS_ERROR;
		}
	}
	return ret;
}

int bctbx_file_close(bctbx_vfs_file_t *pFile) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile) {
		ret = pFile->pMethods->pFuncClose(pFile);
		if (ret != 0) {
			bctbx_error("bctbx_file_close: Error %s freeing file handle anyway", strerror(-(ret)));
		}
	}
	bctbx_free(pFile);
	return ret;
}

int bctbx_file_sync(bctbx_vfs_file_t *pFile) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile) {
		ret = pFile->pMethods->pFuncSync(pFile);
		if (ret != BCTBX_VFS_OK) {
			bctbx_error("bctbx_file_sync: Error %s ", strerror(-(ret)));
		}
	}
	return ret;
}


int64_t bctbx_file_size(bctbx_vfs_file_t *pFile) {
	int64_t ret = BCTBX_VFS_ERROR;
	if (pFile){
		ret = pFile->pMethods->pFuncFileSize(pFile);
		if (ret < 0) bctbx_error("bctbx_file_size: Error file size %s", strerror((int)-(ret)));
	} 
	return ret;
}


int bctbx_file_truncate(bctbx_vfs_file_t *pFile, int64_t size) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile){
		ret = pFile->pMethods->pFuncTruncate(pFile, size);
		if (ret < 0) bctbx_error("bctbx_file_truncate: Error truncate  %s", strerror((int)-(ret)));
	} 
	return ret;
}

ssize_t bctbx_file_fprintf(bctbx_vfs_file_t *pFile, off_t offset, const char *fmt, ...) {
	char *ret = NULL;
	va_list args;
	ssize_t r = BCTBX_VFS_ERROR;
	size_t count = 0;

	va_start(args, fmt);
	ret = bctbx_strdup_vprintf(fmt, args);
	if (ret != NULL) {
		va_end(args);
		count = strlen(ret);

		if (offset != 0) pFile->offset = offset;
		r = bctbx_file_write(pFile, ret, count, pFile->offset);
		bctbx_free(ret);
		if (r > 0) pFile->offset += r;
	}
	return r;
}

/**
 * This function manages the offset stored in pFile and used by the bctbx_file_get_nxtline function
 * It has no effect on the current offset in the underlying file which we have no use of
 */
off_t bctbx_file_seek(bctbx_vfs_file_t *pFile, off_t offset, int whence) {
	off_t ret = BCTBX_VFS_ERROR;

	if (pFile) {
		switch (whence) {
			case SEEK_SET: // set to the given offset
				ret = offset;
				break;
			case SEEK_CUR: // set relative to the current offset
				ret = pFile->offset + offset;
				break;
			case SEEK_END: // set relative to the end of file
				ret = bctbx_file_size(pFile) + offset;
				break;
			default:
				bctbx_error("Encrypted VFS: Invalid whence value in bcSeek: %d", whence);
				return BCTBX_VFS_ERROR;
		}
		pFile->offset = ret;
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

/* a generic implementation of get_nxt_line
 * if a vfs does not specify one, use this one
 */
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
static int bctbx_generic_get_nxtline(bctbx_vfs_file_t *pFile, char *s, int max_len) {
	int64_t ret;
	int sizeofline;
	char *pNextLine = NULL;
	char *pNextLineR = NULL;
	char *pNextLineN = NULL;

	if (!pFile) {
		return BCTBX_VFS_ERROR;
	}
	if (s == NULL || max_len < 1) {
		return BCTBX_VFS_ERROR;
	}

	sizeofline = 0;
	s[max_len-1] = '\0';

	/* Read returns 0 if end of file is found */
	ret = bctbx_file_read(pFile, s, max_len - 1, pFile->offset);
	if (ret > 0) {
		pNextLineR = strchr(s, '\r');
		pNextLineN = strchr(s, '\n');
		if ((pNextLineR != NULL) && (pNextLineN != NULL)) pNextLine = MIN(pNextLineR, pNextLineN);
		else if (pNextLineR != NULL) pNextLine = pNextLineR;
		else if (pNextLineN != NULL) pNextLine = pNextLineN;
		if (pNextLine) {
			/* Got a line! */
			*pNextLine = '\0';
			sizeofline = (int)(pNextLine - s + 1);
			if (pNextLine[1] == '\n') sizeofline += 1; /*take into account the \r\n" case*/

			/* offset to next beginning of line*/
			pFile->offset += sizeofline;
		} else {
			/*did not find end of line char, is EOF?*/
			sizeofline = (int)ret;
			pFile->offset += sizeofline;
			s[ret] = '\0';
		}
	} else if (ret < 0) {
		bctbx_error("bcGetLine error");
	}
	return sizeofline;
}

int bctbx_file_get_nxtline(bctbx_vfs_file_t *pFile, char *s, int maxlen) {
	if (pFile) { /* if the vfs does not implement this method, use the generic one */
		if (pFile->pMethods && pFile->pMethods->pFuncGetLineFromFd) {
			return pFile->pMethods->pFuncGetLineFromFd(pFile, s, maxlen);
		} else {
			return bctbx_generic_get_nxtline(pFile, s, maxlen);
		}
	}
	return BCTBX_VFS_ERROR;
}

bool_t bctbx_file_isEncrypted(bctbx_vfs_file_t *pFile) {
	if (pFile && pFile->pMethods && pFile->pMethods->pFuncIsEncrypted){
		return pFile->pMethods->pFuncIsEncrypted(pFile);
	}
	return FALSE;
}

void bctbx_vfs_set_default(bctbx_vfs_t *my_vfs) {
	pDefaultVfs = my_vfs;
}

bctbx_vfs_t* bctbx_vfs_get_default(void) {
	return pDefaultVfs;	
}

bctbx_vfs_t* bctbx_vfs_get_standard(void) {
	return &bcStandardVfs;
}

