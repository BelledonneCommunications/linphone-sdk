<?php
/*
	x3dh-createBase.php
	@author Johan Pascal
	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// Database access
define("DB_HOST", "mysql");
define("DB_USER", "root");
define("DB_PASSWORD", "root");

$curve = $_GET['curve'];

if ($curve != 448 ) { // Default is curve25519
	$curve=25519;
}

function x3dh_create_db($dbname, $curve) {
	$db = mysqli_connect(DB_HOST, DB_USER, DB_PASSWORD);
	if (!$db) {
		error_log ("Error: Unable to connect to MySQL.\nDebugging errno: " . mysqli_connect_errno() . "\nDebugging error: " . mysqli_connect_error() . "\n");
		exit;
	}

	// get a clean base
	$db->query("DROP DATABASE IF EXISTS $dbname");
	$db->query("CREATE DATABASE $dbname CHARACTER SET utf8 COLLATE utf8_bin");

	// select DB
	$db->select_db($dbname);

	/* Users table:
	 *  - id as primary key (internal use)
	 *  - a userId(shall be the GRUU)
	 *  - Ik (Identity key - EDDSA public key)
	 *  - SPk(the current signed pre-key - ECDH public key)
	 *  - SPk_sig(current SPk public key signed with Identity key)
	 *  - SPh_id (id for SPk provided and internally used by client) - 4 bytes unsigned integer
	 */

	/* One Time PreKey table:
	 *  - an id as primary key (internal use)
	 *  - the Uid of user owning that key
	 *  - the public key (ECDH public key)
	 *  - OPk_id (id for OPk provided and internally used by client) - 4 bytes unsigned integer
	 */

	/* Config table: holds version and curveId parameters:
	 *  - Name: the parameter name (version or curveId)
	 *  - Value: the parameter value (db version scheme or curveId mapped to an integer)
	 */

	$db->query("CREATE TABLE Users (
			Uid INTEGER NOT NULL AUTO_INCREMENT,
			UserId TEXT NOT NULL,
			Ik BLOB NOT NULL,
			SPk BLOB DEFAULT NULL,
			SPk_sig BLOB DEFAULT NULL,
			SPk_id INTEGER UNSIGNED DEFAULT NULL,
			PRIMARY KEY(Uid));");

	$db->query("CREATE TABLE OPk (
			id INTEGER NOT NULL AUTO_INCREMENT,
			Uid INTEGER NOT NULL,
			OPk BLOB NOT NULL,
			OPk_id INTEGER UNSIGNED NOT NULL,
			PRIMARY KEY(id),
			FOREIGN KEY(Uid) REFERENCES Users(Uid) ON UPDATE CASCADE ON DELETE CASCADE);");

	$db->query("CREATE TABLE Config(
			Name VARCHAR(20),
			Value INTEGER NOT NULL);");

	$db->query("INSERT INTO Config(Name, Value) VALUES('version', 0x000001)"); // version of DB scheme MMmmpp
	$db->query("INSERT INTO Config(Name, Value) VALUES('curveId', $curve)"); // what curveId we're using

	$db->close();
}

// Uncomment next line if you really want to use this script to create the requested tables
//x3dh_create_db('x3dh'.$curve, $curve);

?>
