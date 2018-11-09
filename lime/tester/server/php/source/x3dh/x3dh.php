<?php
/*
	x3dh.php
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


function x3dh_get_db_conn() {
	$db = mysqli_connect(DB_HOST, DB_USER, DB_PASSWORD, DB_NAME);
	if (!$db) {
		error_log ("Error: Unable to connect to MySQL.\nDebugging errno: " . mysqli_connect_errno() . "\nDebugging error: " . mysqli_connect_error() . "\n");
		exit;
	}
	return $db;
}


// be sure we do not display any error or it may mess the returned message
ini_set('display_errors', 'Off');

// emulate simple enumeration
abstract class LogLevel {
	const DISABLED = 0;
	const ERROR = 1;
	const WARNING = 2;
	const MESSAGE = 3;
	const DEBUG = 4;
};

function stringErrorLevel($level) {
	switch($level) {
		case LogLevel::DISABLED: return "DISABLED";
		case LogLevel::ERROR: return "ERROR";
		case LogLevel::WARNING: return "WARNING";
		case LogLevel::MESSAGE: return "MESSAGE";
		case LogLevel::DEBUG: return "DEBUG";
		default: return "UNKNOWN";
	}
}

function x3dh_log($level, $message) {
	if (x3dh_logLevel>=$level) {
		if ($level === LogLevel::ERROR) { // in ERROR case, add a backtrace
			file_put_contents(x3dh_logFile, date("Y-m-d h:m:s")." -".x3dh_logDomain."- ".stringErrorLevel($level)." : $message\n".print_r(debug_backtrace(),true), FILE_APPEND);
		} else {
			file_put_contents(x3dh_logFile, date("Y-m-d h:m:s")." -".x3dh_logDomain."- ".stringErrorLevel($level)." : $message\n", FILE_APPEND);
		}
	}
}

/**
 * Constants settings
 */
// define the curve Id used on this server, default is curve25519, it will be either load from DB or got from args if we are creation the DB.
// WARNING: value shall be in sync with defines in client code: Curve25519 = 1, Curve448 = 2 */
// emulate simple enumeration with abstract class
abstract class CurveId {
	const CURVE25519 = 1;
	const CURVE448 = 2;
};

define ('X3DH_protocolVersion', 0x01);
define ('X3DH_headerSize', 3);

// WARNING: value shall be in sync with defines in client code */
// emulate simple enumeration with abstract class
abstract class MessageTypes {
	const unset_type = 0x00;
	const registerUser = 0x01;
	const deleteUser = 0x02;
	const postSPk = 0x03;
	const postOPks = 0x04;
	const getPeerBundle = 0x05;
	const peerBundle = 0x06;
	const getSelfOPks = 0x07;
	const selfOPks = 0x08;
	const error = 0xff;
};

// emulate simple enumeration with abstract class
abstract class ErrorCodes {
	const bad_content_type = 0x00;
	const bad_curve = 0x01;
	const missing_senderId = 0x02;
	const bad_x3dh_protocol_version = 0x03;
	const bad_size = 0x04;
	const user_already_in = 0x05;
	const user_not_found = 0x06;
	const db_error = 0x07;
	const bad_request = 0x08;
	const server_failure = 0x09;
};

// emulate simple enumeration with abstract class
abstract class KeyBundleFlag {
	const noOPk = 0x00;
	const OPk = 0x01;
	const noBundle = 0x02;
}



// define keys and signature size in bytes based on the curve used
const keySizes = array(
	CurveId::CURVE25519 => array ('X_pub' => 32, 'X_priv' => 32, 'ED_pub' => 32, 'ED_priv' => 32, 'Sig' => 64),
	CurveId::CURVE448 => array ('X_pub' => 56, 'X_priv' => 56, 'ED_pub' => 57, 'ED_priv' => 57, 'Sig' => 114)
);

function returnError($code, $errorMessage) {
	x3dh_log(LogLevel::WARNING, "return an error message code ".$code." : ".$errorMessage);
	$header = pack("C4",X3DH_protocolVersion, MessageTypes::error, curveId, $code); // build the X3DH response header, append the error code
	file_put_contents('php://output', $header.$errorMessage);
	exit;
}

function returnOk($message) {
	x3dh_log(LogLevel::DEBUG, "Ok return a message ".bin2hex($message));
	file_put_contents('php://output', $message);
	exit;
}

// Check server setting:
function x3dh_checkSettings() {
	// curveId must be 25519 or 448
	switch(curveId) {
		// Authorized values
		case CurveId::CURVE25519 :
		case CurveId::CURVE448 :
			break;
		// Any other are invalid
		default:
			x3dh_log(LogLevel::ERROR, "Process X3DH request given incorrect curveId parameter (".curveId.").");
			returnError(ErrorCodes::server_failure, "X3DH server is not correctly configured");
	}

	// log Level, if not set or incorrect, disable log
	switch(x3dh_logLevel) {
		// Authorized values
		case LogLevel::DISABLED :
		case LogLevel::ERROR :
		case LogLevel::WARNING :
		case LogLevel::MESSAGE :
		case LogLevel::DEBUG :
			break;
		// any other default to DISABLED
		default:
			error_log("X3DH log level setting is invalid ".x3dh_logLevel.". Disable X3DH logs");
			define ("x3dh_logLevel", LogLevel::DISABLED);
	}
}

function x3dh_process_request($userId) {
	// Check setting
	// that will end the script if settings are detected to be incorrect
	// Could be commented out after server settings have been checked to be ok
	x3dh_checkSettings();

	$userId = filter_var($userId, FILTER_SANITIZE_URL); // userId is supposed to be a GRUU so it shall pass untouched the sanitize URL action
	if ($userId == '') {
		x3dh_log(LogLevel::ERROR, "X3DH server got a request without proper user Id");
		returnError(ErrorCodes::missing_senderId, "User not correctly identified");
	}

	// set response content type
	header('Content-type: x3dh/octet-stream');

	// retrieve response body from php://input
	$request = file_get_contents('php://input');
	$requestSize = strlen($request);
	x3dh_log(LogLevel::DEBUG, "Incoming request from ".$userId." is :".bin2hex($request));

	// check available length before parsing, we must at least have a header
	if ($requestSize < X3DH_headerSize) {
		returnError(ErrorCodes.bad_size, "Packet is not even holding a header. Size ".$requestSize);
	}

	// parse X3DH header
	$X3DH_header=unpack("CprotocolVersion/CmessageType/CcurveId", $request);

	x3dh_log(LogLevel::DEBUG, "protocol version ".$X3DH_header['protocolVersion']." messageType ".$X3DH_header['messageType']." curveId ".$X3DH_header['curveId']);

	if ($X3DH_header['protocolVersion'] != X3DH_protocolVersion) {
		returnError(ErrorCodes::bad_x3dh_protocol_version, "Server running X3DH procotol version ".X3DH_protocolVersion.". Can't process packet with version ".$X3DH_header['protocolVersion']);
	}

	if ($X3DH_header['curveId'] != curveId) {
		returnError(ErrorCodes::bad_curve, "Server running X3DH procotol using curve ".((curveId==CurveId.CURVE25519)?"25519":"448")+"(id ".curveId."). Can't process serve client using curveId ".$X3DH_header['curveId']);
	}

	// acknowledge message by sending an empty message with same header (modified in case of getPeerBundle and get selfOPks request)
	$returnHeader = substr($request, 0, X3DH_headerSize);

	// open the db connexion
	$db = x3dh_get_db_conn();

	switch ($X3DH_header['messageType']) {
		/* Register User Identity Key : Identity Key <EDDSA Public key size >*/
		case MessageTypes::registerUser:
			x3dh_log(LogLevel::MESSAGE, "Got a registerUser Message from ".$userId);
			$x3dh_expectedSize = keySizes[curveId]['ED_pub'];
			if ($requestSize < X3DH_headerSize + $x3dh_expectedSize) {
				returnError(ErrorCodes::bad_size, "Register Identity packet is expexted to be ".(X3DH_headerSize+$x3dh_expectedSize)." bytes, but we got $requestSize bytes");
				return;
			}
			$Ik = substr($request, X3DH_headerSize, $x3dh_expectedSize);

			// Acquire lock on the user table to avoid 2 users being written at the same time with the same name
			$db->query("LOCK TABLES Users WRITE");
			// Check that this usedId is not already in base
			if (($stmt = $db->prepare("SELECT Uid FROM Users WHERE UserId = ? LIMIT 1")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$stmt->bind_param('s', $userId);
			$stmt->execute();
			$result = $stmt->get_result();
			$stmt->close();
			if ($result->num_rows !== 0) {
				$db->query("UNLOCK TABLES");
				returnError(ErrorCodes::user_already_in, "Can't insert user ".$userId." - is already present in base");
			}
			// Insert User in DB
			x3dh_log(LogLevel::DEBUG, "Insert user $userId with Ik :".bin2hex($Ik));
			if (($stmt = $db->prepare("INSERT INTO Users(UserId,Ik) VALUES(?,?)")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$null = NULL;
			$stmt->bind_param('sb', $userId, $null);
			$stmt->send_long_data(1,$Ik);
			$status = $stmt->execute();
			$stmt->close();
			$db->query("UNLOCK TABLES");

			if (!$status) {
				x3dh_log(LogLevel::ERROR, "User $userId INSERT failed error is ".$db->error);
				returnError(ErrorCodes::db_error, "Error while trying to insert user ".userId);
			}

			// we're done
			returnOk($returnHeader);
			break;

		/* Delete user: message is empty(or at least shall be, anyway, just ignore anything present in the messange and just delete the user given in From header */
		case MessageTypes::deleteUser:
			x3dh_log(LogLevel::MESSAGE, "Got a deleteUser Message from ".$userId);
			if (($stmt = $db->prepare("DELETE FROM Users WHERE UserId = ?")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$stmt->bind_param('s', $userId);
			$status = $stmt->execute();
			$stmt->close();

			if ($status) {
				returnOk($returnHeader);
			} else {
				x3dh_log(LogLevel::ERROR, "User $userId DELETE failed err is ".$db->error);
				returnError(ErrorCodes::db_error, "Error while trying to delete user ".$userId);
			}
			break;

		/* Post Signed Pre Key: Signed Pre Key <ECDH public key size> | SPk Signature <Signature length> | SPk id <uint32_t big endian: 4 bytes> */
		case MessageTypes::postSPk:
			x3dh_log(LogLevel::MESSAGE, "Got a postSPk Message from ".$userId);

			$x3dh_expectedSize = keySizes[curveId]['X_pub'] + keySizes[curveId]['Sig'] + 4;
			// check message length
			if ($requestSize < X3DH_headerSize + $x3dh_expectedSize) {
				returnError(ErrorCodes::bad_size, "post SPK packet is expexted to be ".(X3DH_headerSize+$x3dh_expectedSize)." bytes, but we got $requestSize bytes");
			}

			// parse message
			$bufferIndex = X3DH_headerSize;
			$SPk = substr($request, $bufferIndex, keySizes[curveId]['X_pub']);
			$bufferIndex += keySizes[curveId]['X_pub'];
			$Sig = substr($request, $bufferIndex, keySizes[curveId]['Sig']);
			$bufferIndex += keySizes[curveId]['Sig'];
			$SPk_id = unpack('N', substr($request, $bufferIndex))[1]; // SPk id is a 32 bits unsigned integer in Big endian

			// check we have a matching user in DB
			if (($stmt = $db->prepare("SELECT Uid FROM Users WHERE UserId = ? LIMIT 1")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$stmt->bind_param('s', $userId);
			$stmt->execute();
			$result = $stmt->get_result();
			if ($result->num_rows === 0) { // user not found
				$stmt->close();
				returnError(ErrorCodes::user_not_found, "Post SPk but ".$userId." not found in db"); // that will exit the script execution
			}

			// get the Uid from previous query
			$row = $result->fetch_assoc();
			$Uid = $row['Uid'];
			$stmt->close();

			// write the SPk
			if (($stmt = $db->prepare("UPDATE Users SET SPk = ?, SPk_sig = ?, SPk_id = ? WHERE Uid = ?")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$null = NULL;
			$stmt->bind_param('bbii', $null, $null, $SPk_id, $Uid);
			$stmt->send_long_data(0,$SPk);
			$stmt->send_long_data(1,$Sig);

			$status = $stmt->execute();
			$stmt->close();

			if ($status) {
				returnOk($returnHeader);
			} else {
				x3dh_log(LogLevel::ERROR, "User's $userId SPk INSERT failed err is ".$db->error);
				returnError(ErrorCodes::db_error, "Error while trying to insert SPK for user $userId. Backend says :".$db->error);
			}
			break;

		/* Post OPks : OPk number < uint16_t big endian: 2 bytes> | (OPk <ECDH public key size> | OPk id <uint32_t big endian: 4 bytes> ){OPk number} */
		case MessageTypes::postOPks:
			x3dh_log(LogLevel::MESSAGE,"Got a postOPks Message from ".$userId);
			// get the OPks number in the first message bytes(unsigned int 16 in big endian)
			$bufferIndex = X3DH_headerSize;
			$OPk_number = unpack('n', substr($request, $bufferIndex))[1];
			$x3dh_expectedSize = 2 + $OPk_number*(keySizes[curveId]['X_pub'] + 4); // expect: OPk count<2bytes> + number of OPks*(public key size + OPk_Id<4 bytes>)
			if ($requestSize < X3DH_headerSize + $x3dh_expectedSize) {
				returnError(ErrorCodes::bad_size, "post OPKs packet is expected to be ".(X3DH_headerSize+$x3dh_expectedSize)." bytes, but we got $requestSize bytes");
			}
			$bufferIndex+=2; // point to the beginning of first key

			x3dh_log(LogLevel::DEBUG, "It contains ".$OPk_number." keys");

			// check we have a matching user in DB
			if (($stmt = $db->prepare("SELECT Uid FROM Users WHERE UserId = ? LIMIT 1")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$stmt->bind_param('s', $userId);
			$stmt->execute();
			$result = $stmt->get_result();
			if ($result->num_rows === 0) {
				$stmt->close();
				returnError(ErrorCodes::user_not_found, "Post SPk but ".$userId." not found in db"); // that will exit the script execution
			}

			// get the Uid from previous query
			$row = $result->fetch_assoc();
			$Uid = $row['Uid'];
			$stmt->close();

			if (($stmt = $db->prepare("INSERT INTO OPk(Uid, OPk, OPk_id) VALUES(?,?,?)")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$null = NULL;
			$stmt ->bind_param("ibi", $Uid, $null, $OPk_id);

			// before version 5.6.5, begin_transaction doesn't support READ_WRITE flag
			if ($db->server_version < 50605) {
				$db->begin_transaction();
			} else {
				$db->begin_transaction(MYSQLI_TRANS_START_READ_WRITE);
			}
			for ($i = 0; $i<$OPk_number; $i++) {
				$OPk = substr($request, $bufferIndex, keySizes[curveId]['X_pub']);
				$bufferIndex += keySizes[curveId]['X_pub'];
				$stmt->send_long_data(1,$OPk);
				$OPk_id = unpack('N', substr($request, $bufferIndex))[1]; // OPk id is a 32 bits unsigned integer in Big endian
				$bufferIndex += 4;

				if (!$stmt->execute()) {
					$stmt->close();
					$db->rollback();
					returnError(ErrorCodes::db_error, "Error while trying to insert OPk for user ".$userId.". Backend says :".$db->error);
				}
			}

			$stmt->close();
			$db->commit();

			returnOk($returnHeader);
			break;

		/* peerBundle :	bundle Count < 2 bytes unsigned Big Endian> |
		 *	(   deviceId Size < 2 bytes unsigned Big Endian > | deviceId
		 *	    Flag<1 byte: 0 if no OPK in bundle, 1 if OPk is present, 2 when no bundle was found for this device> |
		 *	    Ik <EDDSA Public Key Length> |
		 *	    SPk <ECDH Public Key Length> | SPK id <4 bytes>
		 *	    SPk_sig <Signature Length> |
		 *	    (OPk <ECDH Public Key Length> | OPk id <4 bytes>){0,1 in accordance to flag}
		 *	) { bundle Count}
		 */
		case MessageTypes::getPeerBundle:
			x3dh_log(LogLevel::MESSAGE, "Got a getPeerBundle Message from ".$userId);

			// first parse the message
			if ($requestSize < X3DH_headerSize + 2) { // we must have at least 2 bytes to parse peers counts
				returnError(ErrorCodes::bad_size, "post SPKs packet is expected to be at least ".(X3DH_headerSize+2)." bytes, but we got $requestSize bytes");
			}
			// get the peers number in the first message bytes(unsigned int 16 in big endian)
			$bufferIndex = X3DH_headerSize;
			$peersCount = unpack('n', substr($request, $bufferIndex))[1];
			if ($peersCount === 0) {
				returnError(ErrorCodes::bad_request, "Ask for peer Bundles but no device id given");
			}
			$bufferIndex+=2; // point to the beginning of first key

			// build an array of arrays like [[deviceId], [deviceId], ...]
			$peersBundle = array();
			for ($i=0; $i<$peersCount; $i++) {
				// check we have enought bytes to parse before calling unpack
				if ($requestSize - $bufferIndex<2) {
					returnError(ErrorCodes::bad_request, "Malformed getPeerBundle request: peers count doesn't match the device id list length");
				}
				$idLength = unpack('n', substr($request, $bufferIndex))[1];
				$bufferIndex+=2;
				if ($requestSize - $bufferIndex<$idLength) {
					returnError(ErrorCodes::bad_request, "Malformed getPeerBundle request: device id given size doesn't match the string length");
				}
				$peersBundle[] = array(substr($request, $bufferIndex, $idLength));
				$bufferIndex+=$idLength;
			}

			x3dh_log(LogLevel::DEBUG, "Found request for ".$peersCount." keys bundles");
			x3dh_log(LogLevel::DEBUG, print_r($peersBundle, true));

			$db->query("LOCK TABLES Users,OPk WRITE"); // lock the OPk table so we won't give twice a one-time pre-key

			// before version 5.6.5, begin_transaction doesn't support READ_WRITE flag
			if ($db->server_version < 50605) {
				$db->begin_transaction();
			} else {
				$db->begin_transaction(MYSQLI_TRANS_START_READ_WRITE);
			}

			if (($stmt = $db->prepare("SELECT u.Ik as Ik, u.SPk as SPk, u.SPk_id as SPk_id, u.SPk_sig as SPk_sig, o.OPk as OPk, o.OPk_id as OPk_id, o.id as id FROM Users as u LEFT JOIN OPk as o ON u.Uid=o.Uid WHERE UserId = ? AND u.SPk IS NOT NULL LIMIT 1")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}

			$keyBundleErrors = ''; // an empty string to store error message if any
			for ($i=0; $i<$peersCount; $i++) {
				$stmt->bind_param('s', $peersBundle[$i][0]);
				$stmt->execute();
				$row = $stmt->get_result()->fetch_assoc();
				if (!$row) {
					x3dh_log(LogLevel::DEBUG, "User ".$peersBundle[$i][0]." not found");
				} else {
					array_push($peersBundle[$i], $row['Ik'], $row['SPk'], $row['SPk_id'], $row['SPk_sig']);
					if ($row['OPk']) { // there is an OPk
						array_push($peersBundle[$i], $row['OPk'], $row['OPk_id']);
						// now that we used that one, we MUST delete it
						if (($del_stmt = $db->prepare("DELETE FROM OPk WHERE id = ?")) === FALSE) {
							x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
							returnError(ErrorCodes::db_error, "Server database not ready");
						}
						$del_stmt->bind_param('i', $row['id']);
						if (!$del_stmt->execute()) {
							$keyBundleErrors .=' ## could not delete OPk key retrieved user '.$peersBundle[$i][0]." err : ".$db->error;
						}
						$del_stmt->close();
					}
				}
			}
			$stmt->close();

			// No errors: commit transition, release lock and build the message out of the key bundles extracted
			if($keyBundleErrors === '') {
				$db->commit();
				$db->query("UNLOCK TABLES");
				$peersBundleMessage = pack("C3n",X3DH_protocolVersion, MessageTypes::peerBundle, curveId, $peersCount); // build the X3DH response header
				foreach ($peersBundle as $bundle) {
					// elements in bundle array are : deviceId, [Ik, SPk, SPk_id, SPk_sig, [OPk, OPk_id]]
					$peersBundleMessage .= pack("n", strlen($bundle[0])); // size of peer Device Id, 2 bytes in big endian
					$peersBundleMessage .= $bundle[0]; // peer Device Id
					if (count($bundle)==1) { // we have no keys for this device
						$peersBundleMessage .= pack("C", KeyBundleFlag::noBundle); // 1 byte : the key bundle flag
					} else {
						$flagOPk = (count($bundle)>5)?(KeyBundleFlag::OPk):(KeyBundleFlag::noOPk); // does the bundle hold an OPk?
						$peersBundleMessage .= pack("C", $flagOPk); // 1 byte : the key bundle flag
						$peersBundleMessage .= $bundle[1]; // Ik
						$peersBundleMessage .= $bundle[2]; // SPk
						$peersBundleMessage .= pack("N", $bundle[3]); // SPk Id : 4 bytes in big endian
						$peersBundleMessage .= $bundle[4]; // SPk Signature
						if ($flagOPk == KeyBundleFlag::OPk) {
							$peersBundleMessage .= $bundle[5]; // OPk
							$peersBundleMessage .= pack("N", $bundle[6]); // OPk Id : 4 bytes in big endian
						}
					}
				}
				returnOk($peersBundleMessage);
			} else { // something went wrong
				$db->rollback();
				$db->query("UNLOCK TABLES");
				returnError(ErrorCodes::db_error, "Error while trying to get peers bundles :".$keyBundleErrors);
			}
			break;

		/* selfOPks :	OPKs Id Count < 2 bytes unsigned Big Endian> |
		 *	    (OPk id <4 bytes>){ OPks Id Count}
		 */
		case MessageTypes::getSelfOPks:
			x3dh_log(LogLevel::MESSAGE, "Process a getSelfOPks Message from ".$userId);
			if (($stmt = $db->prepare("SELECT o.OPk_id  as OPk_id FROM Users as u INNER JOIN OPk as o ON u.Uid=o.Uid WHERE UserId = ?")) === FALSE) {
				x3dh_log(LogLevel::ERROR, "Database appears to be not what we expect");
				returnError(ErrorCodes::db_error, "Server database not ready");
			}
			$stmt->bind_param('s', $userId);
			if (!$stmt->execute()) {
				returnError(ErrorCodes::db_error, " Database error in getSelfOPks by ".$userId." : ".$db->error);
			}
			$result = $stmt->get_result();

			// build the X3DH response header, include OPks number (unsigned int on 2 bytes in big endian)
			$selfOPksMessage = pack("C3n",X3DH_protocolVersion, MessageTypes::selfOPks, curveId, $result->num_rows);

			while ($row = $result->fetch_assoc()) {
				$selfOPksMessage .= pack("N", $row['OPk_id']); // OPk Id : 4 bytes in big endian
			}

			returnOk($selfOPksMessage);
			break;

		default:
			returnError(ErrorCodes.bad_message_type, "Unknown message type ".$X3DH_header['messageType']);
			break;
	}

	$db->close();
}
?>
