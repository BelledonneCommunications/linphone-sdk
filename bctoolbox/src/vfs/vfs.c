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

off_t bctbx_file_seek(bctbx_vfs_file_t *pFile, off_t offset, int whence) {
	off_t ret = BCTBX_VFS_ERROR;

	if (pFile) {
		ret = pFile->pMethods->pFuncSeek(pFile, offset, whence);
		if (ret < 0) {
			bctbx_error("bctbx_file_seek: Wrong offset %s", strerror((int)-(ret)));
		} else if (ret == offset) {
			ret = BCTBX_VFS_OK;
		}
	} 
	return ret;
}

int bctbx_file_get_nxtline(bctbx_vfs_file_t *pFile, char *s, int maxlen) {
	if (pFile) return pFile->pMethods->pFuncGetLineFromFd(pFile, s, maxlen);
	return BCTBX_VFS_ERROR;
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

