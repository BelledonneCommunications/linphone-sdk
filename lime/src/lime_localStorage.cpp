/*
	lime_x3dh.cpp
	@author Johan Pascal
	@copyright	Copyright (C) 2017  Belledonne Communications SARL

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

#include <bctoolbox/exception.hh>
#include <soci/soci.h>
#include <set>
#include <mutex>

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_x3dh.hpp"
#include "lime_localStorage.hpp"
#include "lime_double_ratchet.hpp"
#include "lime_impl.hpp"

using namespace::std;
using namespace::soci;
using namespace::lime;

namespace lime {

/******************************************************************************/
/*                                                                            */
/* Db public API                                                              */
/*                                                                            */
/******************************************************************************/
Db::Db(const std::string &filename, std::shared_ptr<std::recursive_mutex> db_mutex) : m_db_mutex{db_mutex} {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	constexpr int db_module_table_not_holding_lime_row = -1;

	int userVersion=db_module_table_not_holding_lime_row;
	try {
		sql.open("sqlite3", filename);
		sql<<"PRAGMA foreign_keys = ON;"; // make sure this connection enable foreign keys
		transaction tr(sql);
		// CREATE OR IGNORE TABLE db_module_version(
		sql<<"CREATE TABLE IF NOT EXISTS db_module_version("
			"name VARCHAR(16) PRIMARY KEY,"
			"version UNSIGNED INTEGER NOT NULL"
			")";
		sql<<"SELECT version FROM db_module_version WHERE name='lime'", into(userVersion);

		// Enforce value in case there is no lime version number in table db_module_version
		if (!sql.got_data()) {
			userVersion=db_module_table_not_holding_lime_row;
		}

		if (userVersion == lime::settings::DBuserVersion) {
			return;
		}

		if (userVersion > lime::settings::DBuserVersion) { /* nothing to do if we encounter a superior version number than expected, just hope it is compatible */
			LIME_LOGE<<"Lime module database schema version found in DB(v "<<userVersion<<") is more recent than the one currently supported by the lime module(v "<<static_cast<unsigned int>(lime::settings::DBuserVersion)<<")";
			return;
		}

		/* Perform update if needed */
		// update the schema version in DB
		if (userVersion == db_module_table_not_holding_lime_row) { // but not any lime row in it
			sql<<"INSERT INTO db_module_version(name,version) VALUES('lime',:DbVersion)", use(lime::settings::DBuserVersion);
		} else { // and we had an older version
			/* Do the update here */
			sql<<"ALTER TABLE lime_LocalUsers ADD COLUMN updateTs DATETIME";
			sql<<"UPDATE lime_LocalUsers SET updateTs = CURRENT_TIMESTAMP";
			// update version number
			sql<<"UPDATE db_module_version SET version = :DbVersion WHERE name='lime'", use(lime::settings::DBuserVersion);
			tr.commit(); // commit all the previous queries
			LIME_LOGI<<"Perform lime database migration from version "<<userVersion<<" to version "<<lime::settings::DBuserVersion;
			return;
		}

		// create the lime DB:
	
		/*** Double Ratchet tables ***/
		/* DR Session:
		*  - DId : link to lime_PeerDevices table, identify which peer is associated to this session
		*  - Uid: link to LocalUsers table, identify which local device is associated to this session
		*  - SessionId(primary key)
		*  - Ns, Nr, PN : index for sending, receivind and previous sending chain
		*  - DHr : peer current public ECDH key
		*  - DHs : self current ECDH key. (public || private keys)
		*  - RK, CKs, CKr : Root key, sender and receiver chain keys
		*  - AD : Associated data : provided once at session creation by X3DH, is derived from initiator public Ik and id, receiver public Ik and id
		*  - Status : 0 is for stale and 1 is for active, only one session shall be active for a peer device, by default created as active
		*  - timeStamp : is updated when session change status and is used to remove stale session after determined time in cleaning operation
		*  - X3DHInit : when we are initiator, store the generated X3DH init message and keep sending it until we've got at least a reply from peer
		*/
		sql<<"CREATE TABLE DR_sessions( \
					Did INTEGER NOT NULL DEFAULT 0, \
					Uid INTEGER NOT NULL DEFAULT 0, \
					sessionId INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
					Ns UNSIGNED INTEGER NOT NULL, \
					Nr UNSIGNED INTEGER NOT NULL, \
					PN UNSIGNED INTEGER NOT NULL, \
					DHr BLOB NOT NULL, \
					DHs BLOB NOT NULL, \
					RK BLOB NOT NULL, \
					CKs BLOB NOT NULL, \
					CKr BLOB NOT NULL, \
					AD BLOB NOT NULL, \
					Status INTEGER NOT NULL DEFAULT 1, \
					timeStamp DATETIME DEFAULT CURRENT_TIMESTAMP, \
					X3DHInit BLOB DEFAULT NULL, \
					FOREIGN KEY(Did) REFERENCES lime_PeerDevices(Did) ON UPDATE CASCADE ON DELETE CASCADE, \
					FOREIGN KEY(Uid) REFERENCES lime_LocalUsers(Uid) ON UPDATE CASCADE ON DELETE CASCADE);";
	
		/* DR Message Skipped DH : Store chains of skipped message keys, this table store the DHr identifying the chain
		*  - DHid (primary key)
		*  - SessionId : foreign key, link to the DR session the skipped keys are attached
		*  - DHr : the peer ECDH public key used in this key chain
		*  - received : count messages successfully decoded since the last MK insertion in that chain, allow to delete chains that are too old
		*/
		sql<<"CREATE TABLE DR_MSk_DHr( \
					DHid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
					sessionId INTEGER NOT NULL DEFAULT 0, \
					DHr BLOB NOT NULL, \
					received UNSIGNED INTEGER NOT NULL DEFAULT 0, \
					FOREIGN KEY(sessionId) REFERENCES DR_sessions(sessionId) ON UPDATE CASCADE ON DELETE CASCADE);";
	
		/* DR Message Skipped MK : Store chains of skipped message keys, this table store the message keys with their index in the chain
		*  - DHid : foreign key, link to the key chain table: DR_Message_Skipped_DH
		*  - Nr : the id in the key chain
		*  - MK : the message key stored
		*  primary key is [DHid,Nr]
		*/
		sql<<"CREATE TABLE DR_MSk_MK( \
					DHid INTEGER NOT NULL, \
					Nr INTEGER NOT NULL, \
					MK BLOB NOT NULL, \
					PRIMARY KEY( DHid , Nr ), \
					FOREIGN KEY(DHid) REFERENCES DR_MSk_DHr(DHid) ON UPDATE CASCADE ON DELETE CASCADE);";
	
		/*** Lime tables : local user identities, peer devices identities ***/
		/* List each self account enable on device :
		*  - Uid : primary key, used to make link with Peer Devices, SPk and OPk tables
		*  - UserId : shall be the GRUU
		*  - Ik : public||private indentity key (EdDSA key)
		*  - server : the URL of key Server
		*  - curveId : identifies the curve used by this user - MUST be in sync with server. This integer stores also the activation byte.
		*  		Mapping is: <Activation byte>||<CurveId byte>
		*  		Activation byte is: 0x00 Active, 0x01 inactive
		*  		CurveId byte: as set in lime.hpp
		*  		default the curveId value to 0 which is not one of the possible values (defined in lime.hpp)
		*  - updateTs : Last update timestamp. When was performed an update operation for this user.
		*/
		sql<<"CREATE TABLE lime_LocalUsers( \
					Uid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
					UserId TEXT NOT NULL, \
					Ik BLOB NOT NULL, \
					server TEXT NOT NULL, \
					curveId INTEGER NOT NULL DEFAULT 0, \
					updateTs DATETIME DEFAULT CURRENT_TIMESTAMP);";
	
		/* Peer Devices :
		* - Did : primary key, used to make link with DR_sessions table.
		* - DeviceId: peer device id (shall be its GRUU)
		* - Ik : Peer device Identity public key, got it from X3DH server or X3DH init message
		* - Status : a flag, 0 : untrusted, 1 : trusted, 2 : unsafe
		*   		The mapping is done in lime.hpp by the PeerDeviceStatus enum class definition
		*
		* Note: peer device information is shared by all local device, hence they are not linked to particular local devices from lime_LocalUsers table
		*
		* Note2: The Ik field should be able to be NULL but it is not for historical reason.
		*        When a peer device is inserted without Ik(through the set_peerDeviceStatus with a unsafe status is the only way to do that)
		*        it will be given an Ik set to invalid_Ik (one byte at 0x00) with the purpose of being unable to match a real Ik as NULL would have done
		*/
		sql<<"CREATE TABLE lime_PeerDevices( \
					Did INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
					DeviceId TEXT NOT NULL, \
					Ik BLOB NOT NULL, \
					Status UNSIGNED INTEGER DEFAULT 0);";
	
		/*** X3DH tables ***/
		/* Signed pre-key :
		* - SPKid : the primary key must be a random number as it is public, so avoid leaking information on number of key used
		* - SPK : Public key||Private Key (ECDH keys)
		* - timeStamp : Application shall renew SPK regurlarly (SPK_LifeTime). Old key are disactivated and deleted after a period (SPK_LimboTime))
		* - Status : a boolean: can be active(1) or stale(0), by default any newly inserted key is set to active
		* - Uid : User Id from lime_LocalUsers table: who's key is this
		*/
		sql<<"CREATE TABLE X3DH_SPK( \
					SPKid UNSIGNED INTEGER PRIMARY KEY NOT NULL, \
					SPK BLOB NOT NULL, \
					timeStamp DATETIME DEFAULT CURRENT_TIMESTAMP, \
					Status INTEGER NOT NULL DEFAULT 1, \
					Uid INTEGER NOT NULL, \
					FOREIGN KEY(Uid) REFERENCES lime_LocalUsers(Uid) ON UPDATE CASCADE ON DELETE CASCADE);";
	
		/* One time pre-key : deleted after usage, generated at user creation and on X3DH server request
		* - OPKid : the primary key must be a random number as it is public, so avoid leaking information on number of key used
		* - OPK : Public key||Private Key (ECDH keys)
		* - Uid : User Id from lime_LocalUsers table: who's key is this
		* - Status : a boolean: is likely to be present on X3DH Server(1), not anymore on X3DH server(0), by default any newly inserted key is set to 1
		* - timeStamp : timeStamp is set during update if we found out a key is no more on server(and we didn't used it as usage delete key).
		*   		So after a limbo period, key is considered missing in action and removed from storage.
		*/
		sql<<"CREATE TABLE X3DH_OPK( \
					OPKid UNSIGNED INTEGER PRIMARY KEY NOT NULL, \
					OPK BLOB NOT NULL, \
					Uid INTEGER NOT NULL, \
					Status INTEGER NOT NULL DEFAULT 1, \
					timeStamp DATETIME DEFAULT CURRENT_TIMESTAMP, \
					FOREIGN KEY(Uid) REFERENCES lime_LocalUsers(Uid) ON UPDATE CASCADE ON DELETE CASCADE);";
	
		tr.commit(); // commit all the previous queries
	} catch (BctbxException const &e) {
		throw BCTBX_EXCEPTION << "Db instanciation on file "<<filename<<" check failed: "<<e.str();
	} catch (exception const &e) {
		throw BCTBX_EXCEPTION << "Db instanciation on file "<<filename<<" check failed: "<<e.what();
	}
};

/**
 * @brief Check for existence, retrieve Uid for local user based on its userId (GRUU) and curve from table lime_LocalUsers
 *
 * @param[in]	deviceId	a string holding the user to look for in DB, shall be its GRUU
 * @param[out]	Uid		the DB internal Id matching given userId (if find in DB, 0 if not find, -1 if found but not active)
 * @param[out]	curveId		the curve selected at user creation
 * @param[out]	url		the url of the X3DH server this user is registered on
 * @param[in]	allStatus	allow loading of inactive user if set to true(default is false)
 *
 */
void Db::load_LimeUser(const std::string &deviceId, long int &Uid, lime::CurveId &curveId, std::string &url, const bool allStatus)
{
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	int curve=0;
	sql<<"SELECT Uid,curveId,server FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(Uid), into(curve), into(url), use(deviceId);

	if (sql.got_data()) { // we found someone
		if (allStatus == false) { // do not allow inactive users to be loaded
			// Check if the user has been activated
			if (curve&lime::settings::DBInactiveUserBit) { // user is inactive
				Uid = -1; // be sure to reset the db_Uid to -1
				throw BCTBX_EXCEPTION << "Lime User "<<deviceId<<" is in DB but has not been activated yet, call create_user again to try to activate";
			}
		}

		// turn back integer value retrieved from DB into a lime::CurveId
		switch (curve&lime::settings::DBCurveIdByte) {
			case static_cast<uint8_t>(lime::CurveId::c25519):
				curveId=lime::CurveId::c25519;
				break;
			case static_cast<uint8_t>(lime::CurveId::c448):
				curveId=lime::CurveId::c448;
				break;
			case static_cast<uint8_t>(lime::CurveId::unset):
			default: // we got an unknow or unset curve Id, DB is either corrupted or a future version
				curveId=lime::CurveId::unset;
				Uid=0;
				throw BCTBX_EXCEPTION << "Lime DB either corrupted or back from the future. User "<<deviceId<<" claim to run with unknown or unset Curve Id "<< curve;
		}
	} else { // no match: throw an execption
		Uid = 0; // be sure to reset the db_Uid to 0
		throw BCTBX_EXCEPTION << "Cannot find Lime User "<<deviceId<<" in DB";
	}
}

/**
 * @brief Delete old stale sessions and old stored message key. Apply to all users in localStorage
 *
 * 	- DR Session in stale status for more than DRSession_limboTime are deleted
 * 	- MessageKey stored linked to a session who received more than maxMessagesReceivedAfterSkip are deleted
 *
 * @note : The messagekeys count is on a chain, so if we have in a chain\n
 * 	Received1 Skip1 Skip2 Received2 Received3 Skip3 Received4\n
 * 	The counter will be reset to 0 when we insert Skip3 (when Received4 arrives) so Skip1 and Skip2 won't be deleted until we got the counter above max on this chain
 * 	Once we moved to next chain(as soon as peer got an answer from us and replies), the count won't be reset anymore
 */
void Db::clean_DRSessions() {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	// WARNING: not sure this code is portable it may work with sqlite3 only
	// delete stale sessions considered to old
	sql<<"DELETE FROM DR_sessions WHERE Status=0 AND timeStamp < date('now', '-"<<lime::settings::DRSession_limboTime_days<<" day');";

	// clean Message keys (MK will be cascade deleted when the DHr is deleted )
	sql<<"DELETE FROM DR_MSk_DHr WHERE received > "<<lime::settings::maxMessagesReceivedAfterSkip<<";";
}

/**
 * @brief Delete old stale SPk. Apply to all users in localStorage
 *
 * SPk in stale status for more than SPK_limboTime_days are deleted
 */
void Db::clean_SPk() {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	// WARNING: not sure this code is portable it may work with sqlite3 only
	// delete stale sessions considered to old
	sql<<"DELETE FROM X3DH_SPK WHERE Status=0 AND timeStamp < date('now', '-"<<lime::settings::SPK_limboTime_days<<" day');";
}

/**
 * @brief set the peer device status flag in local storage: unsafe, trusted or untrusted.
 *
 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
 * @param[in]	Ik		the EdDSA peer public identity key, formatted as in RFC8032
 * @param[in]	status		value of flag to set: accepted values are trusted, untrusted, unsafe
 *
 * @throw 	BCTBX_EXCEPTION	if given key doesn't match the one present in local storage
 *
 * if the status flag value is unexpected (not one of trusted, untrusted, unsafe), ignore the call
 *
 * if the status flag is unsafe or untrusted, ignore the value of Ik and call the version of this function without it
 *
 * if peer Device is not present in local storage and status is trusted or unsafe, it is added, if status is untrusted, it is just ignored
 *
 * General algorithm followed by the set_peerDeviceStatus functions
 * - Status is valid? (not one of trusted, untrusted, unsafe)? No: return
 *
 * - status is trusted
 *       - We have Ik? -> No: return
 *       - Device is already in storage but Ik differs from the given one : exception
 *       - Insert/update in local storage
 *
 * - status is untrusted
 *       - Ik is ignored
 *       - Device already in storage? No: return
 *       - Device already in storage but current status is unsafe? Yes: return
 *       - update in local storage
 *
 * -status is unsafe
 *       - ignore Ik
 *       - insert/update the status. If inserted, insert an invalid Ik
 */
void Db::set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	// if status is unsafe or untrusted, call the variant without Ik
	if (status == lime::PeerDeviceStatus::unsafe || status == lime::PeerDeviceStatus::untrusted) {
		this->set_peerDeviceStatus(peerDeviceId, status);
		return;
	}

	// Check the status flag value, accepted values are: trusted (unsafe and untrusted are already managed)
	if (status != lime::PeerDeviceStatus::trusted) {
		LIME_LOGE<< "Trying to set a status for peer device "<<peerDeviceId<<" which is not acceptable (differs from unsafe, untrusted or trusted), ignore that";
		return;
	}

	LIME_LOGI << "Set status trusted for peer device "<<peerDeviceId;

	const uint8_t statusInteger = static_cast<uint8_t>(lime::PeerDeviceStatus::trusted);

	// Do we have this peerDevice in lime_PeerDevices
	blob Ik_blob(sql);
	long long id;
	sql<<"SELECT Did, Ik FROM Lime_PeerDevices WHERE DeviceId = :peerDeviceId LIMIT 1;", into(id), into(Ik_blob), use(peerDeviceId);
	if (sql.got_data()) { // Found it
		auto IkSize = Ik_blob.get_len();
		std::vector<uint8_t> storedIk;
		storedIk.resize(IkSize);
		Ik_blob.read(0, (char *)(storedIk.data()), IkSize); // Read the public key
		if (storedIk == Ik) {
			sql<<"UPDATE Lime_PeerDevices SET Status = :Status WHERE Did = :id;", use(statusInteger), use(id);
		} else if (IkSize == 1 && storedIk[0] == lime::settings::DBInvalidIk) { // If storedIk is the invalid_Ik, we got it from a setting to unsafe, just replace it with the given one
			blob Ik_update_blob(sql);
			Ik_update_blob.write(0, (char *)(Ik.data()), Ik.size());
			sql<<"UPDATE Lime_PeerDevices SET Status = :Status, Ik = :Ik WHERE Did = :id;", use(statusInteger), use(Ik_update_blob), use(id);
			LIME_LOGW << "Set status trusted for peer device "<<peerDeviceId<<" already present in base without Ik, updated the Ik with provided one";
		} else { // Ik in local Storage differs than the one given... raise an exception
			throw BCTBX_EXCEPTION << "Trying to insert an Identity key for peer device "<<peerDeviceId<<" which differs from one already in local storage";
		}
	} else { // peer is not in local Storage, insert it
		blob Ik_insert_blob(sql);
		Ik_insert_blob.write(0, (char *)(Ik.data()), Ik.size());
		sql<<"INSERT INTO Lime_PeerDevices(DeviceId, Ik, Status) VALUES(:peerDeviceId, :Ik, :Status);", use(peerDeviceId), use(Ik_insert_blob), use(statusInteger);
	}
}

/**
 * @overload
 *
 * Calls with status unsafe or untrusted are executed by this function as they do not need Ik.
 */
void Db::set_peerDeviceStatus(const std::string &peerDeviceId, lime::PeerDeviceStatus status) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	// Check the status flag value, accepted values are: untrusted, unsafe
	if (status != lime::PeerDeviceStatus::unsafe
	&& status != lime::PeerDeviceStatus::untrusted) {
		LIME_LOGE << "Trying to set a status for peer device "<<peerDeviceId<<" without providing a Ik which is not acceptable (differs from unsafe or untrusted)";
		return;
	}
	LIME_LOGI << "Set status "<<((status==lime::PeerDeviceStatus::unsafe)?"unsafe":"untrusted")<<" for peer device "<<peerDeviceId;

	uint8_t statusInteger = static_cast<uint8_t>(status);

	// is this peerDevice already in local storage?
	bool inLocalStorage = false;
	long long id;
	int currentStatus =  static_cast<uint8_t>(lime::PeerDeviceStatus::unsafe);
	sql<<"SELECT Did, Status FROM Lime_PeerDevices WHERE DeviceId = :peerDeviceId;", into(id), into(currentStatus), use(peerDeviceId);
	inLocalStorage = sql.got_data();

	// if status is untrusted
	if (status == lime::PeerDeviceStatus::untrusted) {
		// and we do not already have that device in local storage -> log it and ignore the call
		if (!inLocalStorage) {
			LIME_LOGW << "Trying to set a status untrusted for peer device "<<peerDeviceId<<" not present in local storage, ignore that call)";
			return;
		}
		// and the current status in local storage is already untrusted, do nothing
		if (currentStatus == static_cast<uint8_t>(lime::PeerDeviceStatus::untrusted)) {
			LIME_LOGD << "Set a status untrusted for peer device "<<peerDeviceId<<" but its current status is already untrusted, ignore that call)";
			return;
		}
		// and the current status in local storage is unsafe, keep unsafe
		if (currentStatus == static_cast<uint8_t>(lime::PeerDeviceStatus::unsafe)) {
			LIME_LOGW << "Trying to set a status untrusted for peer device "<<peerDeviceId<<" but its current status is unsafe, ignore that call)";
			return;
		}
	}

	// update or insert
	if (inLocalStorage) {
		sql<<"UPDATE Lime_PeerDevices SET Status = :Status WHERE Did = :id;", use(statusInteger), use(id);
	} else {
		// the lime::settings::DBInvalidIk constant is set into lime_peerDevices table, Ik field when it is not provided by set_peerDeviceStatus as this field can't be set to NULL in older version of the database
		blob Ik_insert_blob(sql);
		Ik_insert_blob.write(0, (char *)(&lime::settings::DBInvalidIk), sizeof(lime::settings::DBInvalidIk));
		sql<<"INSERT INTO Lime_PeerDevices(DeviceId, Ik, Status) VALUES(:peerDeviceId, :Ik, :Status);", use(peerDeviceId), use(Ik_insert_blob), use(statusInteger);
	}
}

/**
 * @brief get the status of a peer device: unknown, untrusted, trusted, unsafe
 * device's Id matching a local account are always considered as trusted
 *
 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
 *
 * @return unknown if the device is not in localStorage, untrusted, trusted or unsafe according to the stored value of peer device status flag otherwise
 */
lime::PeerDeviceStatus Db::get_peerDeviceStatus(const std::string &peerDeviceId) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	if (is_localUser(peerDeviceId)) {
		return lime::PeerDeviceStatus::trusted;
	}
	int status;
	sql<<"SELECT Status FROM Lime_PeerDevices WHERE DeviceId = :peerDeviceId LIMIT 1;", into(status), use(peerDeviceId);
	if (sql.got_data()) { // Found it
		switch (status) {
			case static_cast<uint8_t>(lime::PeerDeviceStatus::untrusted) :
				return lime::PeerDeviceStatus::untrusted;
			case static_cast<uint8_t>(lime::PeerDeviceStatus::trusted) :
				return lime::PeerDeviceStatus::trusted;
			case static_cast<uint8_t>(lime::PeerDeviceStatus::unsafe) :
				return lime::PeerDeviceStatus::unsafe;
			default:
				throw BCTBX_EXCEPTION << "Trying to get the status for peer device "<<peerDeviceId<<" but get an unexpected value "<<status<<" from local storage";
		}
	}

	// peerDeviceId not found in local storage
	return lime::PeerDeviceStatus::unknown;
}

/**
 * @brief get the status of a list of peer device: unknown, untrusted, trusted, unsafe
 * and return the lowest found, crescent order being unsafe, unknown, untrusted, trusted
 * device's Id matching a local account are always considered as trusted
 *
 * @param[in]	peerDeviceIds	A list of devices Id, shall be their GRUUs
 *
 * @return the lowest status found in the list
 */
lime::PeerDeviceStatus Db::get_peerDeviceStatus(const std::list<std::string> &peerDeviceIds) {

	// If there is nothing to search, just return unknown
	if (peerDeviceIds.empty()) return lime::PeerDeviceStatus::unknown;

	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	bool have_untrusted=false;
	size_t found_devices_count =  0;

	// create a comma separated list of device id for the IN parameter in the SQL query as SOCI does not handle a list of it
	std::string sqlString_allDevicesId{""};
	for (const auto &peerDeviceId : peerDeviceIds) {
		sqlString_allDevicesId.append("'").append(peerDeviceId).append("',");
	}
	sqlString_allDevicesId.pop_back(); // remove the last ','
	// Get local devices among the list
	rowset<std::string> rs_localDevices = (sql.prepare << "SELECT l.UserId FROM lime_LocalUsers as l WHERE l.UserId IN ("<<sqlString_allDevicesId<<");");
	std::string sqlString_peerDeviceQuery{"SELECT d.Status FROM lime_PeerDevices as d WHERE d.DeviceId IN ("};

	std::list<std::string> nolocalDevices = peerDeviceIds; // copy original list
	// remove local users from the list: they are all considered as trusted
	for (const std::string &localDevice : rs_localDevices) {
		nolocalDevices.remove(localDevice);
		found_devices_count++;
	}

	if (found_devices_count > 0) {
		// there are local devices, we must use the list without local ones as they can be present both in localUser and PeerDevices
		// but in that case they must be ignored in the second list as they always are considered as trusted
		std::string sqlString_peerDevicesId{""};
		for (const auto &peerDeviceId : nolocalDevices) {
			sqlString_peerDevicesId.append("'").append(peerDeviceId).append("',");
		}
		if (!sqlString_peerDevicesId.empty()) {
			sqlString_peerDevicesId.pop_back(); // remove the last ','
		}
		sqlString_peerDeviceQuery.append(sqlString_peerDevicesId);
	} else {
		// there is no local device in the list, fetch using the whole list
		sqlString_peerDeviceQuery.append(sqlString_allDevicesId);
	}

	rowset<int> rs_devicesStatus = (sql.prepare << sqlString_peerDeviceQuery << ");");
	for (const int status : rs_devicesStatus) {
		found_devices_count++;
		switch (status) {
			case static_cast<uint8_t>(lime::PeerDeviceStatus::trusted) :
				// Do nothing for trusted as it is the higher status we can get
				break;
			case static_cast<uint8_t>(lime::PeerDeviceStatus::untrusted) :
				have_untrusted=true;
				break;
			case static_cast<uint8_t>(lime::PeerDeviceStatus::unsafe) :
				return lime::PeerDeviceStatus::unsafe; // if unsafe is found, it can't get worse, return it
			default : // something is wrong with the local storage
				throw BCTBX_EXCEPTION << "Trying to get the status for peer devices "<<sqlString_allDevicesId<<" but get an unexpected value "<<status<<" from local storage";
		}
	}

	if (found_devices_count != peerDeviceIds.size()) {
		return lime::PeerDeviceStatus::unknown; // we are missing some, return unknown
	}

	if (have_untrusted) return lime::PeerDeviceStatus::untrusted;
	return lime::PeerDeviceStatus::trusted;
}


/**
 * @brief checks if a device Id exists in the local users table
 *
 * @param[in]	deviceId	The device Id
 *
 * @return true if it exists, false otherwise
 */
bool Db::is_localUser(const std::string &deviceId) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	int count = 0;
	sql<<"SELECT count(*) FROM Lime_LocalUsers WHERE UserId = :deviceId LIMIT 1;", into(count), use(deviceId);
	return sql.got_data() && count > 0;
}

/**
 * @brief checks if a device needs to be updated
 * return true if the device exists and updateTs is older than OPk_updatePeriod
 *
 * @param[in]	deviceId	The device Id
 *
 * @return true the updateTs is older than OPk_updatePeriod, false otherwise
 */
bool Db::is_updateRequested(const std::string &deviceId) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	int count = 0;
	sql<<"SELECT count(*) FROM Lime_LocalUsers WHERE UserId = :deviceId AND updateTs < date('now', '-"<<lime::settings::OPk_updatePeriod<<" seconds') LIMIT 1;", into(count), use(deviceId);
	return sql.got_data() && count > 0;
}

/**
 * @brief update the update timestamp to now()
 *
 * @param[in]	deviceId	The device Id
 *
 */
void Db::set_updateTs(const std::string &deviceId) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	sql<<"UPDATE Lime_LocalUsers SET updateTs = CURRENT_TIMESTAMP WHERE UserId = :deviceID", use(deviceId);
}


/**
 * @brief delete a peerDevice from local storage
 *
 * @param[in]	peerDeviceId	The device Id to be removed from local storage, shall be its GRUU
 *
 * Call is silently ignored if the device is not found in local storage
 */
void Db::delete_peerDevice(const std::string &peerDeviceId) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	sql<<"DELETE FROM lime_peerDevices WHERE DeviceId = :peerDeviceId;", use(peerDeviceId);
}

/**
 * @brief Check peer device information(DeviceId - GRUU -, public Ik, Uid to link it to a user) in local storage
 *
 * @param[in] peerDeviceId	The device id to check
 * @param[in] peerIk		The public EDDSA identity key of this device
 * @param[in] updateInvalid	When true, will update the Ik with the given one if the stored one is lime:settings::DBInvalidIk and returns its id.
 *
 * @throws	BCTBX_EXCEPTION	if the device is found in local storage but with a different Ik (if Ik is lime::settings::DBInvalidIk, just pretend we never found the device)
 *
 * @return the id internally used by db to store this row. 0 if this device is not in the local storage or have Ik set to lime::settings::DBInvalidIk
 */
template <typename Curve>
long int Db::check_peerDevice(const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, const bool updateInvalid) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	try {
		blob Ik_blob(sql);
		long int Did=0;

		// make sure this device wasn't already here, if it was, check they have the same Ik
		sql<<"SELECT Ik,Did FROM lime_PeerDevices WHERE DeviceId = :DeviceId LIMIT 1;", into(Ik_blob), into(Did), use(peerDeviceId);
		if (sql.got_data()) { // Found one
			const auto stored_Ik_size = Ik_blob.get_len();
			if (stored_Ik_size == 1) { //Ik seems to be lime::settings::DBInvalidIk, check that
				uint8_t stored_Invalid_Ik = ~lime::settings::DBInvalidIk; // make sure the initial value is not the one we test against
				Ik_blob.read(0, (char *)(&stored_Invalid_Ik), 1); // Read it
				if (stored_Invalid_Ik == lime::settings::DBInvalidIk) { // we stored the invalid Ik
					if (updateInvalid == true) { // We shall update the value with the given Ik and return the Did
						blob Ik_update_blob(sql);
						Ik_update_blob.write(0, (char *)(peerIk.data()), peerIk.size());
						sql<<"UPDATE Lime_PeerDevices SET Ik = :Ik WHERE Did = :id;", use(Ik_update_blob), use(Did);
						LIME_LOGW << "Check peer device status updated empty/invalid Ik for peer device "<<peerDeviceId;
						return Did;
					} else { // just proceed as the key were not in base
						return 0;
					}
				}
			}

			if (stored_Ik_size != peerIk.size()) { // can't match they are not the same size
				LIME_LOGE<<"It appears that peer device "<<peerDeviceId<<" was known with an identity key but is trying to use another one now";
				throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" changed its Ik";
			}
			DSA<Curve, lime::DSAtype::publicKey> stored_Ik;
			Ik_blob.read(0, (char *)(stored_Ik.data()), stored_Ik.size()); // Read it to compare it to the given one
			if (stored_Ik == peerIk) { // they match, so we just return the Did
				return Did;
			} else { // Ik are not matching, peer device changed its Ik!?! Reject
				LIME_LOGE<<"It appears that peer device "<<peerDeviceId<<" was known with an identity key but is trying to use another one now";
				throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" changed its Ik";
			}
		} else { // not found in local Storage: return 0
			return 0;
		}
	} catch (BctbxException const &e) {
		throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" check failed: "<<e.str();
	} catch (exception const &e) {
		throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" check failed: "<<e.what();
	}

}

/**
 * @brief Store peer device information(DeviceId - GRUU -, public Ik, Uid to link it to a user) in local storage
 *
 * @param[in] peerDeviceId	The device id to insert
 * @param[in] peerIk		The public EDDSA identity key of this device
 *
 * @return the id internally used by db to store this row
 */
template <typename Curve>
long int Db::store_peerDevice(const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk) {
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);

	try {
		blob Ik_blob(sql);
		long int Did=0;

		// make sure this device wasn't already here, if it was, check they have the same Ik
		Did = check_peerDevice(peerDeviceId, peerIk, true); // perform checks on peer device and returns its Id if found in local storage already
		if (Did != 0) {
			return Did;
		} else { // not found in local Storage
			Ik_blob.write(0, (char *)(peerIk.data()), peerIk.size());
			sql<<"INSERT INTO lime_PeerDevices(DeviceId,Ik) VALUES (:deviceId,:Ik) ", use(peerDeviceId), use(Ik_blob);
			sql<<"select last_insert_rowid()",into(Did);
			LIME_LOGD<<"store peerDevice "<<peerDeviceId<<" with device id "<<Did;
			return Did;
		}
	} catch (exception const &e) {
		throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" insertion failed: "<<e.what();
	}
}

/**
 * @brief if exists, delete user
 *
 * @param[in]	deviceId	a string holding the user to look for in DB, shall be its GRUU
 *
 */
void Db::delete_LimeUser(const std::string &deviceId)
{
	std::lock_guard<std::recursive_mutex> lock(*m_db_mutex);
	sql<<"DELETE FROM lime_LocalUsers WHERE UserId = :userId;", use(deviceId);
}

/**
 * @brief start a transaction on this Db
 *
 */
void Db::start_transaction()
{
	sql.begin();
}

/**
 * @brief commit a transaction on this Db
 *
 */
void Db::commit_transaction()
{
	sql.commit();
}

/**
 * @brief rollback a transaction on this Db
 *
 */
void Db::rollback_transaction()
{
	try {
		sql.rollback();
	} catch (exception const &e) {
		LIME_LOGE<<"Lime session save transaction rollback failed, backend says: "<<e.what();
	}
}

/* template instanciations for Curves 25519 and 448 */
#ifdef EC25519_ENABLED
	template long int Db::check_peerDevice<C255>(const std::string &peerDeviceId, const DSA<C255, lime::DSAtype::publicKey> &Ik, const bool updateInvalid);
	template long int Db::store_peerDevice<C255>(const std::string &peerDeviceId, const DSA<C255, lime::DSAtype::publicKey> &Ik);
#endif

#ifdef EC448_ENABLED
	template long int Db::check_peerDevice<C448>(const std::string &peerDeviceId, const DSA<C448, lime::DSAtype::publicKey> &Ik, const bool updateInvalid);
	template long int Db::store_peerDevice<C448>(const std::string &peerDeviceId, const DSA<C448, lime::DSAtype::publicKey> &Ik);
#endif

/******************************************************************************/
/*                                                                            */
/*  Lime members functions                                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @brief Create a new local user based on its userId(GRUU) from table lime_LocalUsers
 * The user will be activated only after being published successfully on X3DH server
 *
 * use m_selfDeviceId as input
 * populate m_db_Uid
 *
 * @return true if user was activated successfully.
 *
 * @exception BCTBX_EXCEPTION	thrown if user is not found in base
 */
template <typename Curve>
bool Lime<Curve>::activate_user() {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	// check if the user is the DB
	int Uid = 0;
	int curveId = 0;
	m_localStorage->sql<<"SELECT Uid,curveId FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(Uid), into(curveId), use(m_selfDeviceId);
	if (!m_localStorage->sql.got_data()) {
		throw BCTBX_EXCEPTION << "Lime user "<<m_selfDeviceId<<" cannot be activated, it is not present in local storage";
	}

	transaction tr(m_localStorage->sql);

	// update in DB
	try {
		// Don't create stack variable in the method call directly
		uint8_t curveId = static_cast<int8_t>(Curve::curveId());

		m_localStorage->sql<<"UPDATE lime_LocalUsers SET curveId = :curveId WHERE Uid = :Uid;", use(curveId), use(Uid);
	} catch (exception const &e) {
		tr.rollback();
		throw BCTBX_EXCEPTION << "Lime user activation failed. DB backend says: "<<e.what();
	}
	m_db_Uid = Uid;

	tr.commit();

	return true;
}

template <typename Curve>
void Lime<Curve>::get_SelfIdentityKey() {
	if (m_Ik_loaded == false) {
		std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
		blob Ik_blob(m_localStorage->sql);
		m_localStorage->sql<<"SELECT Ik FROM Lime_LocalUsers WHERE Uid = :UserId LIMIT 1;", into(Ik_blob), use(m_db_Uid);
		if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
			Ik_blob.read(0, (char *)(m_Ik.publicKey().data()), m_Ik.publicKey().size()); // Read the public key
			Ik_blob.read(m_Ik.publicKey().size(), (char *)(m_Ik.privateKey().data()), m_Ik.privateKey().size()); // Read the private key
			m_Ik_loaded = true; // set the flag
		}
	}
}

/**
 * @brief Generate (or load) a batch of OPks, store them in local storage and return their public keys with their ids.
 *
 * @param[out]	OPks		A vector of all the generated (or loaded) OPks
 * @param[in]	OPk_number	How many keys shall we generate. This parameter is ignored if the load flag is set and we find some keys to load
 * @param[in]	load		Flag, if set first try to load keys from storage to return them and if none found just generate the requested amount
 */
template <typename Curve>
void Lime<Curve>::X3DH_generate_OPks(std::vector<OneTimePreKey<Curve>> &OPks, const uint16_t OPk_number, const bool load) {

	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));

	// make room for OPk and OPk ids
	OPks.clear();
	OPks.reserve(OPk_number);

	// OPkIds must be random but unique, prepare them before insertion
	std::set<uint32_t> activeOPkIds{};

	// fetch existing OPk ids from DB (OPKid is unique on all users, so really get them all, do not restrict with m_db_Uid)
	rowset<row> rs = (m_localStorage->sql.prepare << "SELECT OPKid FROM X3DH_OPK");
	for (const auto &r : rs) {
		auto OPk_id = static_cast<uint32_t>(r.get<int>(0));
		activeOPkIds.insert(OPk_id);
	}

	// Shall we try to just load OPks before generating them?
	if (load) {
		blob OPk_blob(m_localStorage->sql);
		uint32_t OPk_id;
		// Prepare DB statement: add a filter on current user Id as we'll target all retrieved OPk_ids (soci doesn't allow rowset and blob usage together)
		// Get Keys matching the currend user and that are not set as dispatched yet (Status = 1)
		statement st = (m_localStorage->sql.prepare << "SELECT OPk FROM X3DH_OPK WHERE Uid = :Uid AND Status = 1 AND OPKid = :OPkId;", into(OPk_blob), use(m_db_Uid), use(OPk_id));

		for (uint32_t id : activeOPkIds) { // We already have all the active OPK ids, loop on them
			OPk_id = id; // copy the id into the bind variable
			st.execute(true);
			if (m_localStorage->sql.got_data()) {
				sBuffer<OneTimePreKey<Curve>::serializedSize()> serializedOPk{};
				OPk_blob.read(0, (char *)(serializedOPk.data()), OneTimePreKey<Curve>::serializedSize());
				OPks.push_back(OneTimePreKey<Curve>(serializedOPk, OPk_id));
			}
		}

		if (OPks.size()>0) { // We found some OPks, all set then
			return;
		}
	}

	// we must create OPk_number new OPks
	// Create an key exchange context to create key pairs
	auto DH = make_keyExchange<Curve>();
	while (OPks.size() < OPk_number){
		// Generate a random OPk Id
		// Sqlite doesn't really support unsigned value, the randomize function makes sure that the MSbit is set to 0 to not fall into strange bugs with that
		uint32_t OPk_id = m_RNG->randomize();

		if (activeOPkIds.insert(OPk_id).second) { // if this Id wasn't in the set, use it
			// Generate a new ECDH Key pair
			DH->createKeyPair(m_RNG);
			// set in output vector
			OPks.emplace_back(DH->get_selfPublic(), DH->get_secret(), OPk_id);
		}
	}

	// Prepare DB statement
	uint32_t OPk_id = 0;
	transaction tr(m_localStorage->sql);
	blob OPk_blob(m_localStorage->sql);
	statement st = (m_localStorage->sql.prepare << "INSERT INTO X3DH_OPK(OPKid, OPK,Uid) VALUES(:OPKid,:OPK,:Uid)", use(OPk_id), use(OPk_blob), use(m_db_Uid));

	try {
		for (const auto &OPk : OPks) { // loop on all OPk
			// Insert in DB: store Public Key || Private Key
			OPk_blob.write(0, (const char *)(OPk.serialize().data()), OneTimePreKey<Curve>::serializedSize());
			OPk_id = OPk.get_Id(); // store also the key id
			st.execute(true);
		}
	} catch (exception &e) {
		OPks.clear();
		tr.rollback();
		throw BCTBX_EXCEPTION << "OPK insertion in DB failed. DB backend says : "<<e.what();
	}
	// commit changes to DB
	tr.commit();
}

template <typename Curve>
void Lime<Curve>::cache_DR_sessions(std::vector<RecipientInfos> &internal_recipients, std::vector<std::string> &missing_devices) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	// build a user list of missing ones : produce a list ready to be sent to SQL query: 'user','user','user',... also build a map to store shared_ptr to sessions
	// build also a list of all peer devices used to fetch from DB their status: unknown, untrusted or trusted
	std::string sqlString_requestedDevices{""};
	std::string sqlString_allDevices{""};

	size_t requestedDevicesCount = 0;
	size_t allDevicesCount = 0;
	// internal recipients holds all recipients
	for (const auto &recipient : internal_recipients) {
		if (recipient.DRSession == nullptr) { // query the local storage for those without DR session associated
			sqlString_requestedDevices.append("'").append(recipient.deviceId).append("',");
			requestedDevicesCount++;
		}
		sqlString_allDevices.append("'").append(recipient.deviceId).append("',");  // we also build a query for all devices in the list
		allDevicesCount++;
	}

	if (allDevicesCount==0) return; // the device list was empty... this is very strange

	sqlString_allDevices.pop_back(); // remove the last ','
	// Fill the peer device status
	rowset<row> rs_devices = (m_localStorage->sql.prepare << "SELECT d.DeviceId, d.Status FROM lime_PeerDevices as d WHERE d.DeviceId IN ("<<sqlString_allDevices<<");");
	std::vector<std::string> knownDevices{}; // vector of known deviceId
	// loop all the found devices and then find them in the internal_recipient vector by looping it until find, this is not at all efficient but
	// shall not be a real problem as recipient list won't get massive(and if they do, this part we not be the blocking one)
	// by default at construction the RecipientInfos object have a peerStatus set to unknown so it will be kept to it for all devices not found in the localStorage
	for (const auto &r : rs_devices) {
		auto deviceId = r.get<std::string>(0);
		auto status = r.get<int>(1);
		for (auto &recipient : internal_recipients) { //look for it in the list
			if (recipient.deviceId == deviceId) {
				switch (status) {
					case static_cast<uint8_t>(lime::PeerDeviceStatus::trusted) :
						recipient.peerStatus = lime::PeerDeviceStatus::trusted;
						break;
					case static_cast<uint8_t>(lime::PeerDeviceStatus::untrusted) :
						recipient.peerStatus = lime::PeerDeviceStatus::untrusted;
						break;
					case static_cast<uint8_t>(lime::PeerDeviceStatus::unsafe) :
						recipient.peerStatus = lime::PeerDeviceStatus::unsafe;
						break;
					default : // something is wrong with the local storage
						throw BCTBX_EXCEPTION << "Trying to get the status for peer device "<<deviceId<<" but get an unexpected value "<<status<<" from local storage";
				}
			}
		}
	}

	// Now do we have sessions to load?
	if (requestedDevicesCount==0) return; // we already got them all

	sqlString_requestedDevices.pop_back(); // remove the last ','

	// fetch them from DB
	rowset<row> rs = (m_localStorage->sql.prepare << "SELECT s.sessionId, d.DeviceId FROM DR_sessions as s INNER JOIN lime_PeerDevices as d ON s.Did=d.Did WHERE s.Uid= :Uid AND s.Status=1 AND d.DeviceId IN ("<<sqlString_requestedDevices<<");", use(m_db_Uid));

	std::unordered_map<std::string, std::shared_ptr<DR>> requestedDevices; // found session will be loaded and temp stored in this
	for (const auto &r : rs) {
		auto sessionId = r.get<int>(0);
		auto peerDeviceId = r.get<std::string>(1);

		auto DRsession = make_DR_from_localStorage<Curve>(m_localStorage, sessionId, m_RNG); // load session from local storage
		requestedDevices[peerDeviceId] = DRsession; // store found session in a our temp container
		m_DR_sessions_cache[peerDeviceId] = DRsession; // session is also stored in cache
	}

	// loop on internal recipient and fill it with the found ones, store the missing ones in the missing_devices vector
	for (auto &recipient : internal_recipients) {
		if (recipient.DRSession == nullptr) { // they are missing
			auto retrievedElem = requestedDevices.find(recipient.deviceId);
			if (retrievedElem == requestedDevices.end()) { // we didn't found this one
				missing_devices.push_back(recipient.deviceId);
			} else { // we got this one
				recipient.DRSession = std::move(retrievedElem->second); // don't need this pointer in tmp comtainer anymore
			}
		}
	}
}

// load from local storage in DRSessions all DR session matching the peerDeviceId, ignore the one picked by id in 2nd arg
template <typename Curve>
void Lime<Curve>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDRSessionId, std::vector<std::shared_ptr<DR>> &DRSessions) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	rowset<int> rs = (m_localStorage->sql.prepare << "SELECT s.sessionId FROM DR_sessions as s INNER JOIN lime_PeerDevices as d ON s.Did=d.Did WHERE d.DeviceId = :senderDeviceId AND s.Uid = :Uid AND s.sessionId <> :ignoreThisDRSessionId ORDER BY s.Status DESC, timeStamp ASC;", use(senderDeviceId), use (m_db_Uid), use(ignoreThisDRSessionId));

	for (const auto &sessionId : rs) {
		/* load session in cache DRSessions */
		DRSessions.push_back(make_DR_from_localStorage<Curve>(m_localStorage, sessionId, m_RNG)); // load session from cache
	}
};

/**
 * @brief retrieve matching SPk from localStorage, throw an exception if not found
 *
 * @param[in]	SPk_id	Id of the SPk we're trying to fetch
 * @return 	The SPk if found
 */
template <typename Curve>
SignedPreKey<Curve> Lime<Curve>::X3DH_get_SPk(uint32_t SPk_id) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	blob SPk_blob(m_localStorage->sql);
	m_localStorage->sql<<"SELECT SPk FROM X3DH_SPk WHERE Uid = :Uid AND SPKid = :SPk_id LIMIT 1;", into(SPk_blob), use(m_db_Uid), use(SPk_id);
	if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
		sBuffer<SignedPreKey<Curve>::serializedSize()> serializedSPk{};
		SPk_blob.read(0, (char *)(serializedSPk.data()), SignedPreKey<Curve>::serializedSize());
		return SignedPreKey<Curve>(serializedSPk, SPk_id);
	} else {
		throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<"look up for SPk id "<<SPk_id<<" failed";
	}
}

/**
 * @brief retrieve matching OPk from localStorage, throw an exception if not found
 * 	Note: once fetch, the OPk is deleted from localStorage
 *
 * @param[in]	OPk_id	Id of the OPk we're trying to fetch
 * @return The OPk if found
 */
template <typename Curve>
OneTimePreKey<Curve> Lime<Curve>::X3DH_get_OPk(uint32_t OPk_id) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	blob OPk_blob(m_localStorage->sql);
	m_localStorage->sql<<"SELECT OPk FROM X3DH_OPK WHERE Uid = :Uid AND OPKid = :OPk_id LIMIT 1;", into(OPk_blob), use(m_db_Uid), use(OPk_id);
	if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
		sBuffer<OneTimePreKey<Curve>::serializedSize()> serializedOPk{};
		OPk_blob.read(0, (char *)(serializedOPk.data()), OneTimePreKey<Curve>::serializedSize());
		return OneTimePreKey<Curve>(serializedOPk, OPk_id);
	} else {
		throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<"look up for OPk id "<<OPk_id<<" failed";
	}
}

/**
 * @brief update OPk Status so we can get an idea of what's on server and what was dispatched but not used yet
 * 	get rid of anyone with status 0 and oldest than OPk_limboTime_days
 *
 * @param[in]	OPkIds	List of Ids found on server
 */
template <typename Curve>
void Lime<Curve>::X3DH_updateOPkStatus(const std::vector<uint32_t> &OPkIds) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	if (OPkIds.size()>0) { /* we have keys on server */
		// build a comma-separated list of OPk id on server
		std::string sqlString_OPkIds{""};
		for (const auto &OPkId : OPkIds) {
			sqlString_OPkIds.append(to_string(OPkId)).append(",");
		}

		sqlString_OPkIds.pop_back(); // remove the last ','

		// Update Status and timeStamp in DB for keys we own and are not anymore on server
		m_localStorage->sql << "UPDATE X3DH_OPK SET Status = 0, timeStamp=CURRENT_TIMESTAMP WHERE Status = 1 AND Uid = :Uid AND OPKid NOT IN ("<<sqlString_OPkIds<<");", use(m_db_Uid);
	} else { /* we have no keys on server */
		m_localStorage->sql << "UPDATE X3DH_OPK SET Status = 0, timeStamp=CURRENT_TIMESTAMP WHERE Status = 1 AND Uid = :Uid;", use(m_db_Uid);
	}

	// Delete keys not anymore on server since too long
	m_localStorage->sql << "DELETE FROM X3DH_OPK WHERE Uid = :Uid AND Status = 0 AND timeStamp < date('now', '-"<<lime::settings::OPk_limboTime_days<<" day');", use(m_db_Uid);
}

template <typename Curve>
void Lime<Curve>::set_x3dhServerUrl(const std::string &x3dhServerUrl) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	transaction tr(m_localStorage->sql);

	// update in DB, do not check presence as we're called after a load_user who already ensure that
	try {
		m_localStorage->sql<<"UPDATE lime_LocalUsers SET server = :server WHERE UserId = :userId;", use(x3dhServerUrl), use(m_selfDeviceId);
	} catch (exception const &e) {
		tr.rollback();
		throw BCTBX_EXCEPTION << "Cannot set the X3DH server url for user "<<m_selfDeviceId<<". DB backend says: "<<e.what();
	}
	// update in the Lime object
	m_X3DH_Server_URL = x3dhServerUrl;

	tr.commit();
}

template <typename Curve>
void Lime<Curve>::stale_sessions(const std::string &peerDeviceId) {
	std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
	transaction tr(m_localStorage->sql);

	// update in DB, do not check presence as we're called after a load_user who already ensure that
	try {
		m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Uid = :Uid AND Status = 1 AND Did = (SELECT Did FROM lime_PeerDevices WHERE DeviceId= :peerDeviceId LIMIT 1)", use(m_db_Uid), use(peerDeviceId);
	} catch (exception const &e) {
		tr.rollback();
		throw BCTBX_EXCEPTION << "Cannot stale sessions between user "<<m_selfDeviceId<<" and user "<<peerDeviceId<<". DB backend says: "<<e.what();
	}

	tr.commit();
}

/* template instanciations for Curves 25519 and 448 */
#ifdef EC25519_ENABLED
	template bool Lime<C255>::activate_user();
	template void Lime<C255>::get_SelfIdentityKey();
	template void Lime<C255>::X3DH_generate_OPks(std::vector<OneTimePreKey<C255>> &OPks, const uint16_t OPk_number, const bool load);
	template void Lime<C255>::cache_DR_sessions(std::vector<RecipientInfos> &internal_recipients, std::vector<std::string> &missing_devices);
	template void Lime<C255>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDBSessionId, std::vector<std::shared_ptr<DR>> &DRSessions);
	template SignedPreKey<C255> Lime<C255>::X3DH_get_SPk(uint32_t SPk_id);
	template OneTimePreKey<C255> Lime<C255>::X3DH_get_OPk(uint32_t OPk_id);
	template void Lime<C255>::X3DH_updateOPkStatus(const std::vector<uint32_t> &OPkIds);
	template void Lime<C255>::set_x3dhServerUrl(const std::string &x3dhServerUrl);
	template void Lime<C255>::stale_sessions(const std::string &peerDeviceId);
#endif

#ifdef EC448_ENABLED
	template bool Lime<C448>::activate_user();
	template void Lime<C448>::get_SelfIdentityKey();
	template void Lime<C448>::X3DH_generate_OPks(std::vector<OneTimePreKey<C448>> &OPks, const uint16_t OPk_number, const bool load);
	template void Lime<C448>::cache_DR_sessions(std::vector<RecipientInfos> &internal_recipients, std::vector<std::string> &missing_devices);
	template void Lime<C448>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDBSessionId, std::vector<std::shared_ptr<DR>> &DRSessions);
	template SignedPreKey<C448> Lime<C448>::X3DH_get_SPk(uint32_t SPk_id);
	template OneTimePreKey<C448> Lime<C448>::X3DH_get_OPk(uint32_t OPk_id);
	template void Lime<C448>::X3DH_updateOPkStatus(const std::vector<uint32_t> &OPkIds);
	template void Lime<C448>::set_x3dhServerUrl(const std::string &x3dhServerUrl);
	template void Lime<C448>::stale_sessions(const std::string &peerDeviceId);
#endif


} // namespace lime
