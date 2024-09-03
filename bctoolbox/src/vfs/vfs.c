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

#include "bctoolbox/crypto.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/vfs.h"
#include "bctoolbox/vfs_standard.h"
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>

static ssize_t bctbx_file_flush(bctbx_vfs_file_t *pFile);

/* Pointer to default VFS initialized to standard VFS implemented here.*/
static bctbx_vfs_t *pDefaultVfs = &bcStandardVfs; /* bcStandardVfs is defined int vfs_standard.h*/

/**
 * Create flags (int) from mode(char*).
 * @param mode Can be r, r+, w+, w, a, a+
 * @return flags (integer).
 */
static int set_flags(const char *mode) {
	int flags = 0; /* flags to pass to open() call */
	char clean_mode[4] = {0};
	size_t i;
	/* drop 'b' mode, no longer useful but we need to be compatible */
	for (i = 0; i < sizeof(clean_mode) - 1 && *mode != '\0'; ++i) {
		if (*mode != 'b') {
			clean_mode[i] = *mode;
			++mode;
		}
	}

	if (strcmp(clean_mode, "r") == 0) {
		flags = O_RDONLY;
	} else if (strcmp(clean_mode, "r+") == 0) {
		flags = O_RDWR;
	} else if (strcmp(clean_mode, "w") == 0) {
		flags = O_WRONLY | O_CREAT | O_TRUNC;
	} else if ((strcmp(clean_mode, "w+") == 0)) {
		flags = O_RDWR | O_CREAT;
	} else if (strcmp(clean_mode, "a") == 0) {
		flags = O_WRONLY | O_APPEND;
	} else if ((strcmp(clean_mode, "a+") == 0)) {
		flags = O_RDWR | O_APPEND;
	} else {
		bctbx_fatal("bctbx_vfs_open(): unsupported open mode '%s'", clean_mode);
	}
#ifdef _WIN32
	flags |= _O_BINARY;
#endif
	return flags;
}

ssize_t bctbx_file_write(bctbx_vfs_file_t *pFile, const void *buf, size_t count, off_t offset) {
	ssize_t ret;

	if (pFile != NULL) {
		if (bctbx_file_flush(pFile) < 0) { // make sure our write is not overwritten by a page flush
			return BCTBX_VFS_ERROR;
		}

		ret = pFile->pMethods->pFuncWrite(pFile, buf, count, offset);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_write file error");
			return BCTBX_VFS_ERROR;
		} else if (ret < 0) {
			bctbx_error("bctbx_file_write error %s", strerror((int)-ret));
			return BCTBX_VFS_ERROR;
		}
		pFile->gSize = 0; // cancel get cache, as it might be dirty now
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

ssize_t bctbx_file_write2(bctbx_vfs_file_t *pFile, const void *buf, size_t count) {
	ssize_t ret = bctbx_file_write(pFile, buf, count, pFile->offset);
	if (ret != BCTBX_VFS_ERROR) {
		bctbx_file_seek(pFile, (off_t)ret, SEEK_CUR);
	}
	return ret;
}

static int file_open(bctbx_vfs_t *pVfs, bctbx_vfs_file_t *pFile, const char *fName, const int oflags) {
	int ret = BCTBX_VFS_ERROR;
	if (pVfs && pFile) {
		ret = pVfs->pFuncOpen(pVfs, pFile, fName, oflags);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_open: Error file handle");
		} else if (ret < 0) {
			bctbx_error("bctbx_file_open: Error opening '%s': %s", fName, strerror((int)-ret));
			ret = BCTBX_VFS_ERROR;
		}
	}
	return ret;
}

bctbx_vfs_file_t *bctbx_file_open(bctbx_vfs_t *pVfs, const char *fName, const char *mode) {
	int ret;
	bctbx_vfs_file_t *p_ret = (bctbx_vfs_file_t *)bctbx_malloc(sizeof(bctbx_vfs_file_t));
	int oflags = set_flags(mode);
	if (p_ret) {
		memset(p_ret, 0, sizeof(bctbx_vfs_file_t));
		ret = file_open(pVfs, p_ret, fName, oflags);
		if (ret == BCTBX_VFS_OK) return p_ret;
	}

	if (p_ret) bctbx_free(p_ret);
	return NULL;
}

bctbx_vfs_file_t *bctbx_file_open2(bctbx_vfs_t *pVfs, const char *fName, const int openFlags) {
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

static ssize_t bctbx_file_flush(bctbx_vfs_file_t *pFile) {
	if (pFile->fSize == 0) {
		return 0;
	}
	size_t fSize = pFile->fSize; // save the size so we could restore it if something goes wrong
	pFile->fSize =
	    0; // set it to 0 now so when we call write it won't enter in an infinite loop(as write will call flush)
	ssize_t r = bctbx_file_write(pFile, pFile->fPage, fSize, pFile->fPageOffset);
	if (r < 0) { // something went wrong, restore the page size
		pFile->fSize = fSize;
	}
	return r;
}

ssize_t bctbx_file_read(bctbx_vfs_file_t *pFile, void *buf, size_t count, off_t offset) {
	ssize_t ret = BCTBX_VFS_ERROR;
	if (pFile) {
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}

		ret = pFile->pMethods->pFuncRead(pFile, buf, count, offset);
		/*check if error : in this case pErrSvd is initialized*/
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("bctbx_file_read: error bctbx_vfs_file_t");
		} else if (ret < 0) {
			bctbx_error("bctbx_file_read: Error read %s", strerror((int)-ret));
			ret = BCTBX_VFS_ERROR;
		}
	}
	return ret;
}

ssize_t bctbx_file_read2(bctbx_vfs_file_t *pFile, void *buf, size_t count) {
	ssize_t ret = bctbx_file_read(pFile, buf, count, pFile->offset);
	if (ret != BCTBX_VFS_ERROR) {
		bctbx_file_seek(pFile, (off_t)ret, SEEK_CUR);
	}
	return ret;
}

int bctbx_file_close(bctbx_vfs_file_t *pFile) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile) {
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}
		/* clean the fprint and getline cache as they might hold the plain version of an encrypted file */
		if (bctbx_file_is_encrypted(pFile)) {
			bctbx_clean(pFile->fPage, BCTBX_VFS_PRINTF_PAGE_SIZE);
			bctbx_clean(pFile->gPage, BCTBX_VFS_GETLINE_PAGE_SIZE);
		}

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
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}

		ret = pFile->pMethods->pFuncSync(pFile);
		if (ret != BCTBX_VFS_OK) {
			bctbx_error("bctbx_file_sync: Error %s ", strerror(-(ret)));
		}
	}
	return ret;
}

ssize_t bctbx_file_size(bctbx_vfs_file_t *pFile) {
	ssize_t ret = BCTBX_VFS_ERROR;
	if (pFile) {
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}

		ret = pFile->pMethods->pFuncFileSize(pFile);
		if (ret < 0) bctbx_error("bctbx_file_size: Error file size %s", strerror((int)-(ret)));
	}
	return ret;
}

int bctbx_file_truncate(bctbx_vfs_file_t *pFile, int64_t size) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile) {
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}

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

		if (offset != 0) {
			bctbx_file_flush(pFile);
			pFile->offset = offset;
		}

		// Shall we write in cache or in the file
		if (count + pFile->fSize < BCTBX_VFS_PRINTF_PAGE_SIZE) { // Data fits in current page
			memcpy(pFile->fPage + pFile->fSize, ret, count);
			if (pFile->fSize == 0) {
				pFile->fPageOffset = pFile->offset;
			}
			bctbx_free(ret);
			pFile->offset += (off_t)count;
			pFile->fSize += count;
			pFile->gSize = 0; // cancel get cache, as it might be dirty now
			return (ssize_t)count;
		} else if (pFile->fSize >
		           0) { // There is a cache but the new data won't fit in : write the cache and the new data
			char *buf = bctbx_malloc(
			    count +
			    pFile->fSize); // allocate a temporary buffer, to store the current cache and the data to be written
			if (buf == NULL) {
				bctbx_free(ret);
				return BCTBX_VFS_ERROR;
			}
			memcpy(buf, pFile->fPage, pFile->fSize); // concatenate the cache and new data
			memcpy(buf + pFile->fSize, ret, count);
			bctbx_free(ret);

			// We are flushing the cache along the new data to write
			size_t fSizeBkp = pFile->fSize; // save the size so we could restore it if something goes wrong
			pFile->fSize = 0;               // cache empty: so the write try to flush it as we are already doing it
			r = bctbx_file_write(pFile, buf, fSizeBkp + count, pFile->fPageOffset); // write all
			bctbx_free(buf);
			if (r < 0) {
				pFile->fSize = fSizeBkp;
				return r;
			}
			pFile->offset += (off_t)count;
			return (ssize_t)count;
		}
		// no cache and more than one page to write, just write it
		r = bctbx_file_write(pFile, ret, count, pFile->offset);
		bctbx_free(ret);
		if (r > 0) pFile->offset += (off_t)r;
	}
	return r;
}

/**
 * This function manages the offset stored in pFile and used by the bctbx_file_get_nxtline function
 * It has no effect on the current offset in the underlying file which we have no use of
 */
off_t bctbx_file_seek(bctbx_vfs_file_t *pFile, off_t offset, int whence) {
	off_t ret = BCTBX_VFS_ERROR;

	if (bctbx_file_flush(pFile) < 0) {
		return BCTBX_VFS_ERROR;
	}

	if (pFile) {
		switch (whence) {
			case SEEK_SET: // set to the given offset
				ret = offset;
				break;
			case SEEK_CUR: // set relative to the current offset
				ret = pFile->offset + offset;
				break;
			case SEEK_END: // set relative to the end of file
				ret = (off_t)bctbx_file_size(pFile) + offset;
				break;
			default:
				bctbx_error("bctbx_file_seek(): Invalid whence value: %d", whence);
				return BCTBX_VFS_ERROR;
		}
		pFile->offset = ret;
		return ret;
	}
	return BCTBX_VFS_ERROR;
}

static char *findNextLine(const char *buf) {
	char *pNextLine = NULL;
	char *pNextLineR = NULL;
	char *pNextLineN = NULL;
	pNextLineR = strchr(buf, '\r');
	pNextLineN = strchr(buf, '\n');
	if ((pNextLineR != NULL) && (pNextLineN != NULL)) pNextLine = MIN(pNextLineR, pNextLineN);
	else if (pNextLineR != NULL) pNextLine = pNextLineR;
	else if (pNextLineN != NULL) pNextLine = pNextLineN;
	return pNextLine;
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

	if (!pFile) {
		return BCTBX_VFS_ERROR;
	}
	if (s == NULL || max_len < 1) {
		return BCTBX_VFS_ERROR;
	}
	if (bctbx_file_flush(pFile) < 0) {
		return BCTBX_VFS_ERROR;
	}

	sizeofline = 0;

	// If we have a cached page and the current offset is in this page
	if ((pFile->gSize > 0) && (pFile->gPageOffset <= pFile->offset) &&
	    (pFile->gPageOffset + (off_t)pFile->gSize > pFile->offset)) {
		// look for a new line in the cache
		const char *c = pFile->gPage + (pFile->offset - pFile->gPageOffset);
		pNextLine = findNextLine(c);
		if (pNextLine) {
			/* Got a line! Comments use \n to describe EOL while it can be \r too */
			sizeofline = (int)(pNextLine - c + 1); // 1 one so it actually includes the \n char
			pFile->offset += sizeofline;           // offset to next beginning of line
			if ((pNextLine[0] == '\r') &&
			    (pNextLine[1] == '\n')) { // take into account the \r\n case, this case can pass underdetected if page
				                          // ends in between, not a problem we will just get an extra empty line
				pFile->offset += 1;
			}
			memcpy(s, c, sizeofline - 1); // copy all before the \n
			s[sizeofline - 1] = '\0';     // add a \0 at the end where the \n was
			return sizeofline; // return size including the termination, so an empty line returns 1 (0 is for EOF)
		} else {               // No end of line found, did we reach the EOF?
			if (pFile->gPage[pFile->gSize - 1] ==
			    0x04) { // 0x04 is EOT in ASCII, put in cache to signal the end of file
				sizeofline = (int)(pFile->gSize - (pFile->offset - pFile->gPageOffset) -
				                   1); // size does not include the EOT char(which is part of the page size). so -1
				pFile->offset += sizeofline; // offset now points on the EOT char
				memcpy(s, c, sizeofline);    // copy everything before the EOT
				s[sizeofline] = '\0';
				return sizeofline; // Can be 0 if we were already pointing on EOT when we enter this call
			}
		}
	}

	// not in cache, read it from file
	s[max_len - 1] = '\0';
	/* Read returns 0 if end of file is found */
	ret = bctbx_file_read(pFile, s, max_len - 1, pFile->offset);
	if (ret > 0) {
		size_t readSize = (size_t)ret;
		// Store it in cache
		if (max_len - 1 < BCTBX_VFS_GETLINE_PAGE_SIZE) {
			memcpy(pFile->gPage, s, readSize);
			pFile->gPageOffset = pFile->offset;
			pFile->gSize = readSize;
			if (ret < max_len - 1) {           // read did not return as much as asked, we reached EOF
				pFile->gPage[readSize] = 0x04; // 0x04 is ASCII for EOT, use it to store the EOF in the buffer
				pFile->gSize += 1;             // the EOT is part of the cached page
			}
			pFile->gPage[pFile->gSize] = '\0';
		} else {
			bctbx_warning("bctbx_get_nxtline given a max size value %d bigger than cache size (%d), please adjust one "
			              "or the other",
			              max_len, BCTBX_VFS_GETLINE_PAGE_SIZE);
		}
		pNextLine = findNextLine(s);
		if (pNextLine) {
			/* Got a line! */
			sizeofline = (int)(pNextLine - s + 1);
			/* offset to next beginning of line*/
			pFile->offset += sizeofline;
			if ((pNextLine[0] == '\r') && (pNextLine[1] == '\n')) { /*take into account the \r\n" case*/
				pFile->offset += 1;
			}
			*pNextLine = '\0';
		} else {
			/*did not find end of line char, is EOF?*/
			sizeofline = (int)ret;
			pFile->offset += sizeofline;
			s[readSize] = '\0';
		}
	} else if (ret < 0) {
		bctbx_error("bcGetLine error");
	}
	return sizeofline;
}

int bctbx_file_get_nxtline(bctbx_vfs_file_t *pFile, char *s, int maxlen) {
	if (pFile) { /* if the vfs does not implement this method, use the generic one */
		if (bctbx_file_flush(pFile) < 0) {
			return BCTBX_VFS_ERROR;
		}

		if (pFile->pMethods && pFile->pMethods->pFuncGetLineFromFd) {
			return pFile->pMethods->pFuncGetLineFromFd(pFile, s, maxlen);
		} else {
			return bctbx_generic_get_nxtline(pFile, s, maxlen);
		}
	}
	return BCTBX_VFS_ERROR;
}

bool_t bctbx_file_is_encrypted(bctbx_vfs_file_t *pFile) {
	if (pFile && pFile->pMethods && pFile->pMethods->pFuncIsEncrypted) {
		return pFile->pMethods->pFuncIsEncrypted(pFile);
	}
	return FALSE;
}

void bctbx_vfs_set_default(bctbx_vfs_t *my_vfs) {
	pDefaultVfs = my_vfs;
}

bctbx_vfs_t *bctbx_vfs_get_default(void) {
	return pDefaultVfs;
}

bctbx_vfs_t *bctbx_vfs_get_standard(void) {
	return &bcStandardVfs;
}
