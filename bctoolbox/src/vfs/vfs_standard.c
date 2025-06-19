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

#include "bctoolbox/defs.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/vfs.h"
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>

/**
 * Opens the file with filename fName, associate it to the file handle pointed
 * by pFile, sets the methods bctbx_io_methods_t to the bcio structure
 * and initializes the file size.
 * Sets the error in pErrSvd if an error occurred while opening the file fName.
 * @param  pVfs    		Pointer to  bctx_vfs  VFS.
 * @param  fName   		Absolute path filename.
 * @param  openFlags    Flags to use when opening the file.
 * @return         		BCTBX_VFS_ERROR if an error occurs, BCTBX_VFS_OK otherwise.
 */
static int bcOpen(bctbx_vfs_t *pVfs, bctbx_vfs_file_t *pFile, const char *fName, int openFlags);

/* User data for the standard vfs */
typedef struct bctbx_vfs_standard_t bctbx_vfs_standard_t;
struct bctbx_vfs_standard_t {
	int fd; /* File descriptor */
};

bctbx_vfs_t bcStandardVfs = {
    "bctbx_vfs", /* vfsName */
    bcOpen,      /*xOpen */
};

/**
 * Closes file by closing the associated file descriptor.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile 	bctbx_vfs_file_t File handle pointer.
 * @return       	BCTBX_VFS_OK if successful, BCTBX_VFS_ERROR otherwise.
 */
static int bcClose(bctbx_vfs_file_t *pFile) {
	int ret;
	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;
	ret = close(ctx->fd);
	if (!ret) {
		ret = BCTBX_VFS_OK;
	} else {
		ret = -errno;
	}
	bctbx_free(pFile->pUserData);
	return ret;
}

/**
 * Simply sync the file contents given through the file handle
 * to the persistent media.
 * @param  pFile  File handle pointer.
 * @return   BCTBX_VFS_OK on success, BCTBX_VFS_ERROR otherwise
 */
static int bcSync(bctbx_vfs_file_t *pFile) {
	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;
#if _WIN32
	int ret;
	ret = FlushFileBuffers((HANDLE)_get_osfhandle(ctx->fd));
	return (ret != 0 ? BCTBX_VFS_OK : BCTBX_VFS_ERROR);
#else
	int rc = fsync(ctx->fd);
	return (rc == 0 ? BCTBX_VFS_OK : BCTBX_VFS_ERROR);
#endif
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
static ssize_t bcRead(bctbx_vfs_file_t *pFile, void *buf, size_t count, off_t offset) {
	ssize_t nRead; /* Return value from read() */
	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;

	if (lseek(ctx->fd, offset, SEEK_SET) < 0) {
		if (errno) return -errno;
	} else {
		nRead = bctbx_read(ctx->fd, buf, count);
		/* Error while reading */
		if (nRead < 0) {
			if (errno) return -errno;
		}
		return nRead;
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Writes directly to the open file given through the pFile argument.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile       bctbx_vfs_file_t File handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @return         number of bytes written (can be 0), negative value errno if an error occurred.
 */
static ssize_t bcWrite(bctbx_vfs_file_t *pFile, const void *buf, size_t count, off_t offset) {
	ssize_t nWrite = 0; /* Return value from write() */

	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;

	if ((lseek(ctx->fd, offset, SEEK_SET)) < 0) {
		if (errno) return -errno;
	} else {
		nWrite = bctbx_write(ctx->fd, buf, count);
		if (nWrite >= 0) return nWrite;
		else if (nWrite == (ssize_t)-1) {
			if (errno) return -errno;
			return 0;
		}
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Returns the file size associated with the file handle pFile.
 * @param pFile File handle pointer.
 * @return -errno if an error occurred, file size otherwise (can be 0).
 */
static ssize_t bcFileSize(bctbx_vfs_file_t *pFile) {
	int rc;            /* Return code from fstat() call */
	struct stat sStat; /* Output of fstat() call */
	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;

	rc = fstat(ctx->fd, &sStat);
	if (rc != 0) {
		return -errno;
	}
	return (ssize_t)sStat.st_size;
}

/*
 ** Truncate a file
 * @param pFile File handle pointer.
 * @param new_size Extends the file with null bytes if it is superiori to the file's size
 *                 truncates the file otherwise.
 * @return -errno if an error occurred, 0 otherwise.
 */
static int bcTruncate(bctbx_vfs_file_t *pFile, int64_t new_size) {

	int ret;
	if (pFile == NULL || pFile->pUserData == NULL) return BCTBX_VFS_ERROR;
	bctbx_vfs_standard_t *ctx = (bctbx_vfs_standard_t *)pFile->pUserData;

#if _WIN32
	ret = _chsize(ctx->fd, (long)new_size);
#else
	ret = ftruncate(ctx->fd, new_size);
#endif

	if (ret < 0) {
		return -errno;
	}
	return 0;
}

static const bctbx_io_methods_t bcio = {
    bcClose,          /* pFuncClose */
    bcRead,           /* pFuncRead */
    bcWrite,          /* pFuncWrite */
    bcTruncate,       /* pFuncTruncate */
    bcFileSize,       /* pFuncFileSize */
    bcSync,     NULL, /* use the generic implementation of getnxt line */
    NULL              /* pFuncIsEncrypted -> no function so we will return false */
};

static int bcOpen(BCTBX_UNUSED(bctbx_vfs_t *pVfs), bctbx_vfs_file_t *pFile, const char *fName, int openFlags) {
	if (pFile == NULL || fName == NULL) {
		return BCTBX_VFS_ERROR;
	}
#ifdef _WIN32
	openFlags |= O_BINARY;
#endif

	/* Create the userData structure */
	bctbx_vfs_standard_t *userData = (bctbx_vfs_standard_t *)bctbx_malloc(sizeof(bctbx_vfs_standard_t));
	userData->fd = open(fName, openFlags, S_IRUSR | S_IWUSR);
	if (userData->fd == -1) {
		bctbx_free(userData);
		return -errno;
	}

	pFile->pMethods = &bcio;
	pFile->pUserData = (void *)userData;
	return BCTBX_VFS_OK;
}
