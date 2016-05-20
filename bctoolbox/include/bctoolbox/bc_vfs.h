/*
bc_vfs.h
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

#ifndef bc_vfs_h
#define bc_vfs_h
#include <fcntl.h>

#include <bctoolbox/port.h>
#include <stdbool.h>


#if !defined(_WIN32_WCE)
#include <sys/types.h>
#include <sys/stat.h>
#if _MSC_VER
#include <io.h>
#endif
#endif /*_WIN32_WCE*/


#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32	

#ifndef S_IRUSR
#define S_IRUSR S_IREAD
#endif

#ifndef S_IWUSR
#define S_IWUSR S_IWRITE
#endif

#define open _open
#define read _read
#define write _write
#define close _close
#define lseek _lseek

#endif /*!_WIN32*/

#define BCTBX_VFS_OK           0   /* Successful result */

#define BCTBX_VFS_ERROR       -255   /* Some kind of disk I/O error occurred */


/**
 * Methods associated with the bctbx_vfs_t.
 */
typedef struct bctbx_io_methods bctbx_io_methods;

/**
 * VFS file handle.
 */
typedef struct bctbx_vfs_file_t bctbx_vfs_file_t;
struct bctbx_vfs_file_t {
	const struct bctbx_io_methods *pMethods;  /* Methods for an open file: all implementors must supply this field at open step*/
	/*the fields below are used by the default implementation. Implementors are not required to supply them, but may use them if they find
	 * them useful*/
	uint64_t size;					/*size of file*/
	int fd;                         		/* File descriptor */
	off_t offset;								/*File offset used by lseek*/
};


/**
 */
struct bctbx_io_methods {
	int (*pFuncClose)(bctbx_vfs_file_t *pFile);
	int (*pFuncRead)(bctbx_vfs_file_t *pFile, void* buf, int count, uint64_t offset);
	int (*pFuncWrite)(bctbx_vfs_file_t *pFile , const void* buf, int count, uint64_t offset );
	int (*pFuncFileSize)(bctbx_vfs_file_t *pFile);
	int (*pFuncGetLineFromFd)(bctbx_vfs_file_t *pFile , char* s, int count);
	int (*pFuncSeek)(bctbx_vfs_file_t *pFile, uint64_t offset, int whence);
};


/**
 * VFS definition
 */
typedef struct bctbx_vfs_t bctbx_vfs_t;
struct bctbx_vfs_t {
	bctbx_vfs_t *pNext;      		/* Next registered VFS */
	const char *vfsName;       /* Virtual file system name */
	int (*pFuncFopen)(bctbx_vfs_t* pVfs, bctbx_vfs_file_t *pFile, const char *fName,  const int openFlags);

	
};


/* API to use the VFS */
bctbx_vfs_t *bc_create_vfs(void);
int bctbx_vfs_register(bctbx_vfs_t* pVfs, bctbx_vfs_t** pToVfs);
int bctbx_file_read(bctbx_vfs_file_t* pFile, void *buf, int count, uint64_t offset);
int bctbx_file_close(bctbx_vfs_file_t* pFile);
bctbx_vfs_file_t* bctbx_file_create_and_open(bctbx_vfs_t* pVfs, const char *fName,  const char* mode);

bctbx_vfs_file_t* bctbx_file_create_and_open2(bctbx_vfs_t* pVfs, const char *fName,  const int openFlags );
int bctbx_file_open(bctbx_vfs_t* pVfs, bctbx_vfs_file_t*pFile, const char *fName,  const int oflags);
uint64_t bctbx_file_size(bctbx_vfs_file_t *pFile);
int bctbx_file_write(bctbx_vfs_file_t* pFile, const void *buf, int count, uint64_t offset);
int bctbx_file_fprintf(bctbx_vfs_file_t* pFile, uint64_t offset, const char* fmt, ...);
int bctbx_file_get_nxtline(bctbx_vfs_file_t* pFile, char*s , int maxlen);
int bctbx_file_seek(bctbx_vfs_file_t *pFile, uint64_t offset, int whence);
const bctbx_io_methods* get_bcio(void);

#endif
