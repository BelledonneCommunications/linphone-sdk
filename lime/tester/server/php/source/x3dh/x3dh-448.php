<?php
/*
	x3dh-448.php
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




include "x3dh.php";

/*** X3DH server settings ***/
// Curve to use, shall be either CurveId::CURVE25519 or CurveId::CURVE25519
define ("curveId", CurveId::CURVE448);

// Database access
define("DB_HOST", "mysql");
define("DB_USER", "root");
define("DB_PASSWORD", "root");
// this database must already exists with the requested tables
define ("DB_NAME", "x3dh448");

// log level one of (LogLevel::DISABLED, ERROR, WARNING. MESSAGE, DEBUG)
// default to DISABLED (recommended value)
define ("x3dh_logLevel" , LogLevel::MESSAGE);
define ("x3dh_logFile", "/source/var/log/X3DH448.log"); // make sure to have actual write permission to this file
define ("x3dh_logDomain", "X3DH"); // in case Logs are mixed with other applications ones, format is [time tag] -Domain- message

/*** End of X3DH server settings ***/

// That one shall be set by the user authentication layer
$userId = $_SERVER['HTTP_FROM'];

x3dh_process_request($userId);

?>
