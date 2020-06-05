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

void basic_test() {
	/* register sqlite3_vfs as default */
	sqlite3_bctbx_vfs_register(1);

	/* create a database */
	sqlite3 *db=NULL;
	char *dbFile = bc_tester_file("sqlite3_vfs_basic.sqlite");;
	remove(dbFile);
	bctbx_sqlite3_open(dbFile, &db, NULL);

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
		sqlite3_bctbx_vfs_unregister();
		BC_FAIL("Unable to create table1");
		return;
	}


	/* close databse */
	sqlite3_close(db);

	/* unregister the bxtbx_vfs or it will stay as default one */
	sqlite3_bctbx_vfs_unregister();

	/* cleaning */
	remove(dbFile);
}

static test_t sqlite3_vfs_tests[] = {
	TEST_NO_TAG("basic access", basic_test)
};

test_suite_t sqlite3_vfs_test_suite = {"Sqlite3_vfs", NULL, NULL, NULL, NULL,
							   sizeof(sqlite3_vfs_tests) / sizeof(sqlite3_vfs_tests[0]), sqlite3_vfs_tests};

