/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone.
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

#ifndef BCTBX_SQLITE3_VFS_H
#define BCTBX_SQLITE3_VFS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <bctoolbox/vfs.h>

#include "sqlite3.h"


#define BCTBX_SQLITE3_VFS "sqlite3bctbx_vfs"

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 512

#ifdef __cplusplus
extern "C"{
#endif

/**
 * sqlite3_bctbx_file_t VFS file structure.
 */
typedef struct sqlite3_bctbx_file_t sqlite3_bctbx_file_t;
struct sqlite3_bctbx_file_t {
	sqlite3_file base;              /* Base class. Must be first. */
	bctbx_vfs_file_t* pbctbx_file;
	char *filename;
};



/**
 * Very simple VFS structure based on sqlite3_vfs. 
 * Only the Open function is implemented, 
 */
typedef struct sqlite3_bctbx_vfs_t sqlite3_bctbx_vfs_t;
struct sqlite3_bctbx_vfs_t {
	sqlite3_bctbx_vfs_t *pNext;      /* Next registered VFS */
	const char *vfsName;       /* Virtual file system name */
	int (*xOpen)(sqlite3_vfs* pVfs, const char *fName, sqlite3_file *pFile,int flags, int *pOutFlags);
	
};


/****************************************************
VFS API to register this VFS to sqlite3 VFS
*****************************************************/

/**
 * Registers sqlite3bctbx_vfs to SQLite VFS. If makeDefault is 1,
 * the VFS will be used by default.
 * Methods not implemented by sqlite3_bctbx_vfs_t are initialized to the one 
 * used by the unix-none VFS where all locking file operations are no-ops. 
 * @param  makeDefault  set to 1 to make the newly registered VFS be the default one, set to 0 instead.
 */
BCTBX_PUBLIC void sqlite3_bctbx_vfs_register(int makeDefault);


/**
 * Unregisters sqlite3bctbx_vfs from SQLite.
 */
BCTBX_PUBLIC void sqlite3_bctbx_vfs_unregister(void);

/*
 * Helper function to open a db file
 *
 * @param[in] 	db_file		path to the db file to open/create
 * @param[out]	db		pointer to the sqlite3 db opened
 * @param[in]	vfs_name	if not null sqlite uses this virtual file system instead of the default one
 *
 **/
BCTBX_PUBLIC int bctbx_sqlite3_open(const char *db_file, sqlite3 **db, const char *vfs_name);

#ifdef __cplusplus
}
#endif

#endif /* BCTBX_SQLITE3_VFS_H */
