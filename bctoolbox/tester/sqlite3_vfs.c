/*
 * Copyright (c) 2020 Belledonne Communications SARL.
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

#include <stdio.h>
#include "bctoolbox_tester.h"
#include "bctoolbox/sqlite3_vfs.h"
#include "sqlite3.h"

static int bctbx_sqlite3_open(const char *db_file, sqlite3 **db) {
	char* errmsg = NULL;
	int ret;
	int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;

#if TARGET_OS_IPHONE
	/* the secured filesystem of the iPHone doesn't allow writing while the app is in background mode, which is problematic.
	 * We workaround by asking that the open is made with no protection*/
	flags |= SQLITE_OPEN_FILEPROTECTION_NONE;
#endif

	ret = sqlite3_open_v2(db_file, db, flags, NULL);

	if (ret != SQLITE_OK) return ret;
	// Some platforms do not provide a way to create temporary files which are needed
	// for transactions... so we work in memory only
	// see http ://www.sqlite.org/compile.html#temp_store
	ret = sqlite3_exec(*db, "PRAGMA temp_store=MEMORY", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		sqlite3_free(errmsg);
	}

	return ret;
}

void basic_test() {
	/* register sqlite3_vfs as default */
	sqlite3_bctbx_vfs_register(1);


	/* create a database */
	sqlite3 *db=NULL;
	char *dbFile = bc_tester_file("sqlite3_vfs_basic.sqlite");;
	remove(dbFile);
	bctbx_sqlite3_open(dbFile, &db);

	/* insert a table */
	char* errmsg=NULL;
	int ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS table1 ("
							"id		INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
							"a_blob		BLOB NOT NULL DEFAULT '000000000000',"
							"some_text	TEXT NOT NULL DEFAULT 'unset',"
							"an_int		INTEGER DEFAULT 0"
						");",
			0,0,&errmsg);

	if(ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		BC_FAIL("Unable to create table1");
		return;
	}


	/* close databse */
	sqlite3_close(db);

	/* cleaning */
	remove(dbFile);
}

static test_t sqlite3_vfs_tests[] = {
	TEST_NO_TAG("basic access", basic_test)
};

test_suite_t sqlite3_vfs_test_suite = {"Sqlite3_vfs", NULL, NULL, NULL, NULL,
							   sizeof(sqlite3_vfs_tests) / sizeof(sqlite3_vfs_tests[0]), sqlite3_vfs_tests};

