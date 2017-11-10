/*
	lime_x3dh.cpp
	Copyright (C) 2017  Belledonne Communications SARL

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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BCTBX_LOG_DOMAIN "lime"
#include <bctoolbox/logging.h>

#include "lime/lime.hpp"
#include "lime_localStorage.hpp"
#include "lime_double_ratchet.hpp"
#include "lime_impl.hpp"
#include "bctoolbox/exception.hh"

#include "soci/sqlite3/soci-sqlite3.h"
using namespace::std;
using namespace::soci;
using namespace::lime;

namespace lime {

Db::Db(std::string filename) : sql{sqlite3, filename}{
	int userVersion=-1;
	sql<<"PRAGMA user_version;",into(userVersion);
	sql<<"PRAGMA foreign_keys = ON;"; // make sure this connection enable foreign keys
	if (userVersion != lime::settings::DBuserVersion) {
		if (userVersion > lime::settings::DBuserVersion) { /* nothing to do if we encounter a superior version number than expected, just hope it is compatible */
			//TODO: Log this event, throw an execption?
			//TODO: use a table for versionning not the user_version pragma
		} else { /* Perform update if needed */
			// update the schema version in DB metadata, the above line shall work but if fails at runtime in soci lib on syntax error
			//sql<<"PRAGMA user_version = :versionNumber;", use(lime::settings::DBuserVersion);
			sql<<"PRAGMA user_version = "<<lime::settings::DBuserVersion<<";"; // This one does the job. This pragma is valid in sqlite3 but might not be with other soci backends

			// whole DB creation:
			transaction tr(sql); // begin a transaction which will hold all the create table queries (no sense in allowing the DB to be partially created)

			/*** Double Ratchet tables ***/
			/* DR Session:
			 *  - DId : Device Id is a foreign key from lime_PeerDevices table: peer device Id, allow to retrieve sessions linking to a peer device and a local account
			 *  - SessionId(primary key)
			 *  - Ns, Nr, PN : index for sending, receivind and previous sending chain
			 *  - DHr : peer current public ECDH key
			 *  - DHs public, DHs private : self current ECDH key
			 *  - RK, CKs, CKr : Root key, sender and receiver chain keys
			 *  - AD : Associated data(provided once at session creation by X3DH, shall be initiator public Ik||receiver public Ik)
			 *  - Status : 0 is for stale and 1 is for active, only one session shall be active for a peer device, by default created as active
			 *  - timeStamp : is updated when session change status and is used to remove stale session after determined time in cleaning operation
			 *  - X3DHInit : when we are initiator, store the generated X3DH init message and keep sending it until we've got at least a reply from peer
			 */
			sql<<"CREATE TABLE DR_sessions( \
						Did INTEGER NOT NULL DEFAULT 0, \
						sessionId INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
						Ns UNSIGNED INTEGER NOT NULL, \
						Nr UNSIGNED INTEGER NOT NULL, \
						PN UNSIGNED INTEGER NOT NULL, \
						DHr BLOB NOT NULL, \
						DHs_pub BLOB NOT NULL, \
						DHs_priv BLOB NOT NULL, \
						RK BLOB NOT NULL, \
						CKs BLOB NOT NULL, \
						CKr BLOB NOT NULL, \
						AD BLOB NOT NULL, \
						Status INTEGER NOT NULL DEFAULT 1, \
						timeStamp DATETIME DEFAULT CURRENT_TIMESTAMP, \
						X3DHInit BLOB DEFAULT NULL, \
						FOREIGN KEY(Did) REFERENCES lime_PeerDevices(Did) ON UPDATE CASCADE ON DELETE CASCADE)";

			/* DR Message Skipped DH : DHid(primary key), SessionId, DHr */
			sql<<"CREATE TABLE DR_MSk_DHr( \
						DHid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
						sessionId INTEGER NOT NULL DEFAULT 0, \
						DHr BLOB NOT NULL);";

			/* DR Message Skipped MK : [DHid,NR](primary key), MK */
			sql<<"CREATE TABLE DR_MSk_MK( \
						DHid INTEGER NOT NULL, \
						Nr INTEGER NOT NULL, \
						MK BLOB NOT NULL, \
						PRIMARY KEY( DHid , Nr ));";

			/*** Lime tables : local user identities, peer devices identities ***/
			/* List each self account enable on device :
			   - Uid : primary key, used to make link with Peer Devices, SPk and OPk tables
			   - User Id : shall be the GRUU
			   - Ik : public||private indentity key
			   - server : the URL of key Server
			   - curveId : identifies the curve used by this user - MUST be in sync with server
			*/
			sql<<"CREATE TABLE lime_LocalUsers( \
						Uid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
						UserId TEXT NOT NULL, \
						Ik BLOB NOT NULL, \
						server TEXT NOT NULL, \
						curveId INTEGER NOT NULL DEFAULT 0);"; // default the curveId value to 0 which is not one of the possible values(defined in lime.hpp)

			/* Peer Devices :
			 * - Did : primary key, used to make link with DR_sessions table.
			 * - DeviceId: peer device id (shall be its GRUU)
			 * - Uid: link to LocalUsers table, identify which localUser registered this peer Device
			 * - Ik : Peer device Identity public key, got it from X3DH server or X3DH init message
			 */
			sql<<"CREATE TABLE lime_PeerDevices( \
						Did INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, \
						DeviceId TEXT NOT NULL, \
						Uid INTEGER NOT NULL, \
						Ik BLOB NOT NULL, \
						FOREIGN KEY(Uid) REFERENCES lime_LocalUsers(Uid) ON UPDATE CASCADE ON DELETE CASCADE);";

			/*** X3DH tables ***/
			/* Signed pre-key :
			 * - SPKid : the primary key must be a random number as it is public, so avoid leaking information on number of key used
			 * - SPK : Public key||Private Key (ECDH keys)
			 * - timeStamp : Application shall renew SPK regurlarly(SPK_LifeTime). Old key are disactivated and deleted after a period(SPK_LimboTime))
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
			 */
			sql<<"CREATE TABLE X3DH_OPK( \
						OPKid UNSIGNED INTEGER PRIMARY KEY NOT NULL, \
						OPK BLOB NOT NULL, \
						Uid INTEGER NOT NULL, \
						FOREIGN KEY(Uid) REFERENCES lime_LocalUsers(Uid) ON UPDATE CASCADE ON DELETE CASCADE);";

			tr.commit(); // commit all the previous queries
		}
	}
};
/******************************************************************************/
/*                                                                            */
/* Db public API                                                              */
/*                                                                            */
/******************************************************************************/
/**
 * @brief Check for existence, retrieve Uid for local user based on its userId(GRUU) and curve from table lime_LocalUsers
 *
 * @param[in]	userId		a string holding the user to look for in DB, shall be its GRUU
 * @param[out]	Uid		the DB internal Id matching given userId(if find in DB, 0 otherwise)
 * @param[out]	curveId		the curve selected at user creation
 *
 */
void Db::load_LimeUser(const std::string &userId, long int &Uid, lime::CurveId &curveId, std::string &url)
{
	int curve=0;
	sql<<"SELECT Uid,CurveId,server FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(Uid), into(curve), into(url), use(userId);

	if (sql.got_data()) { // we found someone
		// turn back integer value retrieved from DB into a lime::CurveId
		switch (curve) {
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
				throw BCTBX_EXCEPTION << "Lime DB either corrupted or back from the future. User "<<userId<<" claim to run with unknown or unset Curve Id "<< curve;
		}
	} else { // no match: throw an execption
		Uid = 0; // be sure to reset the db_Uid to 0
		throw BCTBX_EXCEPTION << "Cannot find Lime User "<<userId<<" in DB";
	}
}

/**
 * @brief if exists, delete user
 *
 * @param[in]	userId		a string holding the user to look for in DB, shall be its GRUU
 *
 */
void Db::delete_LimeUser(const std::string &userId)
{
	sql<<"DELETE FROM lime_LocalUsers WHERE UserId = :userId;", use(userId);
}

/******************************************************************************/
/*                                                                            */
/* Double ratchet member functions                                            */
/*                                                                            */
/******************************************************************************/
template <typename DHKey>
bool DR<DHKey>::session_save() {

	// open transaction
	transaction tr(m_localStorage->sql);

	// shall we try to insert or update?
	bool MSk_DHr_Clean = false; // flag use to signal the need for late cleaning in DR_MSk_DHr table
	if (m_dbSessionId==0) { // We have no id for this session row, we shall insert a new one
		// Build blobs from DR session
		blob DHr(m_localStorage->sql);
		DHr.write(0, (char *)(m_DHr.data()), m_DHr.size());
		blob DHs_pub(m_localStorage->sql);
		DHs_pub.write(0, (char *)(m_DHs.publicKey().data()), m_DHs.publicKey().size());
		blob DHs_priv(m_localStorage->sql);
		DHs_priv.write(0, (char *)(m_DHs.privateKey().data()), m_DHs.privateKey().size());
		blob RK(m_localStorage->sql);
		RK.write(0, (char *)(m_RK.data()), m_RK.size());
		blob CKs(m_localStorage->sql);
		CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());
		blob CKr(m_localStorage->sql);
		CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());
		/* this one is written in base only at creation and never updated again */
		blob AD(m_localStorage->sql);
		AD.write(0, (char *)(m_sharedAD.data()), m_sharedAD.size());

		// make sure we have no other session active with this peer DiD
		m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did", use(m_peerDid);

		if (m_X3DH_initMessage.size()>0) {
			blob X3DH_initMessage(m_localStorage->sql);
			X3DH_initMessage.write(0, (char *)(m_X3DH_initMessage.data()), m_X3DH_initMessage.size());
			m_localStorage->sql<<"INSERT INTO DR_sessions(Ns,Nr,PN,DHr,DHs_pub,DHs_priv,RK,CKs,CKr,AD,Did,X3DHInit) VALUES(:Ns,:Nr,:PN,:DHr,:DHs_pub,:DHs_priv,:RK,:CKs,:CKr,:AD,:Did,:X3DHinit);", use(m_Ns), use(m_Nr), use(m_PN), use(DHr), use(DHs_pub), use(DHs_priv), use(RK), use(CKs), use(CKr), use(AD), use(m_peerDid), use(X3DH_initMessage);
		} else {
			m_localStorage->sql<<"INSERT INTO DR_sessions(Ns,Nr,PN,DHr,DHs_pub,DHs_priv,RK,CKs,CKr,AD,Did) VALUES(:Ns,:Nr,:PN,:DHr,:DHs_pub,:DHs_priv,:RK,:CKs,:CKr,:AD,:Did);", use(m_Ns), use(m_Nr), use(m_PN), use(DHr), use(DHs_pub), use(DHs_priv), use(RK), use(CKs), use(CKr), use(AD), use(m_peerDid);
		}
		// if insert went well we shall be able to retrieve the last insert id to save it in the Session object
		/*** WARNING: unportable section of code, works only with sqlite3 backend ***/
		m_localStorage->sql<<"select last_insert_rowid()",into(m_dbSessionId);
		/*** above could should work but it doesn't, consistently return false from .get_last_insert_id... ***/
		/*if (!(sql.get_last_insert_id("DR_sessions", m_dbSessionId))) {
			throw;
		} */
	} else { // we have an id, it shall already be in the db
		// Try to update an existing row
		try{ //TODO: make sure the update was a success, or we shall signal it
			switch (m_dirty) {
				case DRSessionDbStatus::dirty: // dirty case shall actually never occurs as a dirty is set only at creation not loading, first save is processed above
				case DRSessionDbStatus::dirty_ratchet: // ratchet&decrypt modifies all but also request to delete X3DHInit from storage
				{
					// make sure we have no other session active with this peer DiD
					if (m_active_status == false) {
						m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did", use(m_peerDid);
						m_active_status = true;
					}

					// Build blobs from DR session
					blob DHr(m_localStorage->sql);
					DHr.write(0, (char *)(m_DHr.data()), m_DHr.size());
					blob DHs_pub(m_localStorage->sql);
					DHs_pub.write(0, (char *)(m_DHs.publicKey().data()), m_DHs.publicKey().size());
					blob DHs_priv(m_localStorage->sql);
					DHs_priv.write(0, (char *)(m_DHs.privateKey().data()), m_DHs.privateKey().size());
					blob RK(m_localStorage->sql);
					RK.write(0, (char *)(m_RK.data()), m_RK.size());
					blob CKs(m_localStorage->sql);
					CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());
					blob CKr(m_localStorage->sql);
					CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());

					m_localStorage->sql<<"UPDATE DR_sessions SET Ns= :Ns, Nr= :Nr, PN= :PN, DHr= :DHr,DHs_pub= :DHs_pub, DHs_priv= :DHs_priv,RK= :RK, CKs= :CKs, CKr= :CKr, Status = 1,  X3DHInit = NULL WHERE sessionId = :sessionId;", use(m_Ns), use(m_Nr), use(m_PN), use(DHr), use(DHs_pub), use(DHs_priv), use(RK), use(CKs), use(CKr), use(m_dbSessionId);
				}
					break;
				case DRSessionDbStatus::dirty_decrypt: // decrypt modifies: CKr and Nr. Also set Status to active and clear X3DH init message if there is one(it is actually useless as our first reply from peer shall trigger a ratchet&decrypt)
				{
					// make sure we have no other session active with this peer DiD
					if (m_active_status == false) {
						m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did", use(m_peerDid);
						m_active_status = true;
					}

					blob CKr(m_localStorage->sql);
					CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());
					m_localStorage->sql<<"UPDATE DR_sessions SET Nr= :Nr, CKr= :CKr, Status = 1, X3DHInit = NULL WHERE sessionId = :sessionId;", use(m_Nr), use(CKr), use(m_dbSessionId);
				}
					break;
				case DRSessionDbStatus::dirty_encrypt: // encrypt modifies: CKs and Ns
				case DRSessionDbStatus::clean: // encrypt modifies: CKs and Ns
				{
					blob CKs(m_localStorage->sql);
					CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());
					m_localStorage->sql<<"UPDATE DR_sessions SET Ns= :Ns, CKs= :CKs WHERE sessionId = :sessionId;", use(m_Ns), use(CKs), use(m_dbSessionId);
				}
					break;
				default: // Session is clean? So why have we been called?
					bctbx_error("Double ratchet session saved call on sessionId %ld but sessions appears to be clean", m_dbSessionId);
					break;
			}
		} catch (...) {
			throw;
		}
		// updatesert went well, do we have any mkskipped row to modify
		if (m_usedDHid !=0 ) { // ok, we consumed a key, remove it from db
			m_localStorage->sql<<"DELETE from DR_MSk_MK WHERE DHid = :DHid AND Nr = :Nr;", use(m_usedDHid), use(m_usedNr);
			MSk_DHr_Clean = true; // flag the cleaning needed in DR_MSk_DH table, we may have to remove a row in it if no more row are linked to it in DR_MSk_MK
		}
	}

	// Shall we insert some skipped Message keys?
	for ( auto rChain : m_mkskipped) { // loop all chains of message keys, each one is a DHr associated to an unordered map of MK indexed by Nr to be saved
		blob DHr(m_localStorage->sql);
		DHr.write(0, (char *)(rChain.DHr.data()), rChain.DHr.size());
		long DHid=0;
		m_localStorage->sql<<"SELECT DHid FROM DR_MSk_DHr WHERE sessionId = :sessionId AND DHr = :DHr LIMIT 1",into(DHid), use(m_dbSessionId), use(DHr);
		if (!m_localStorage->sql.got_data()) { // There is no row in DR_MSk_DHr matching this key, we must add it
			m_localStorage->sql<<"INSERT INTO DR_MSk_DHr(sessionId, DHr) VALUES(:sessionId, :DHr)", use(m_dbSessionId), use(DHr);
			m_localStorage->sql<<"select last_insert_rowid()",into(DHid); // WARNING: unportable code, sqlite3 only, see above for more details on similar issue
		}
		// insert all the skipped key in the chain
		uint32_t Nr;
		blob MK(m_localStorage->sql);
		statement st = (m_localStorage->sql.prepare << "INSERT INTO DR_MSk_MK(DHid,Nr,MK) VALUES(:DHid,:Nr,:Mk)", use(DHid), use(Nr), use(MK));

		for (const auto &kv : rChain.messageKeys) { // messageKeys is an unordered map of MK indexed by Nr.
			Nr=kv.first;
			MK.write(0, (char *)kv.second.data(), kv.second.size());
			st.execute(true);
		}
	}

	// Now do the cleaning(remove unused row from DR_MKs_DHr table) if needed
	if (MSk_DHr_Clean == true) {
		uint32_t Nr;
		m_localStorage->sql<<"SELECT Nr from DR_MSk_MK WHERE DHid = :DHid LIMIT 1;", into(Nr), use(m_usedDHid);
		if (!m_localStorage->sql.got_data()) { // no more MK with this DHid, remove it
			m_localStorage->sql<<"DELETE from DR_MSk_DHr WHERE DHid = :DHid;", use(m_usedDHid);
		}
	}

	tr.commit();
	return true;
};

template <typename DHKey>
bool DR<DHKey>::session_load() {
	// blobs to store DR session data
	blob DHr(m_localStorage->sql);
	blob DHs_pub(m_localStorage->sql);
	blob DHs_priv(m_localStorage->sql);
	blob RK(m_localStorage->sql);
	blob CKs(m_localStorage->sql);
	blob CKr(m_localStorage->sql);
	blob AD(m_localStorage->sql);
	blob X3DH_initMessage(m_localStorage->sql);

	// create an empty DR session
	indicator ind;
	int status; // retrieve an int from DB, turn it into a bool to store in object
	m_localStorage->sql<<"SELECT Did,Ns,Nr,PN,DHr,DHs_pub,DHs_priv,RK,CKs,CKr,AD,Status,X3DHInit FROM DR_sessions WHERE sessionId = :sessionId LIMIT 1", into(m_peerDid), into(m_Ns), into(m_Nr), into(m_PN), into(DHr), into(DHs_pub), into(DHs_priv), into(RK), into(CKs), into(CKr), into(AD), into(status), into(X3DH_initMessage,ind), use(m_dbSessionId);

	if (m_localStorage->sql.got_data()) { // TODO : some more specific checks on length of retrieved data?
		DHr.read(0, (char *)(m_DHr.data()), m_DHr.size());
		DHs_pub.read(0, (char *)(m_DHs.publicKey().data()), m_DHs.publicKey().size());
		DHs_priv.read(0, (char *)(m_DHs.privateKey().data()), m_DHs.privateKey().size());
		RK.read(0, (char *)(m_RK.data()), m_RK.size());
		CKs.read(0, (char *)(m_CKs.data()), m_CKs.size());
		CKr.read(0, (char *)(m_CKr.data()), m_CKr.size());
		AD.read(0, (char *)(m_sharedAD.data()), m_sharedAD.size());
		if (ind == i_ok && X3DH_initMessage.get_len()>0) {
			m_X3DH_initMessage.resize(X3DH_initMessage.get_len());
			X3DH_initMessage.read(0, (char *)(m_X3DH_initMessage.data()), m_X3DH_initMessage.size());
		}
		if (status==1) {
			m_active_status = true;
		} else {
			m_active_status = false;
		}
		return true;
	} else { // something went wrong with the DB, we cannot retrieve the session
		return false;
	}
};

template <typename Curve>
bool DR<Curve>::trySkippedMessageKeys(const uint32_t Nr, const X<Curve> &DHr, DRMKey &MK) {
	blob MK_blob(m_localStorage->sql);
	blob DHr_blob(m_localStorage->sql);
	DHr_blob.write(0, (char *)(DHr.data()), DHr.size());

	indicator ind;
	m_localStorage->sql<<"SELECT m.MK, m.DHid FROM DR_MSk_MK as m INNER JOIN DR_MSk_DHr as d ON d.DHid=m.DHid WHERE d.sessionId = :sessionId AND d.DHr = :DHr AND m.Nr = :Nr LIMIT 1", into(MK_blob,ind), into(m_usedDHid), use(m_dbSessionId), use(DHr_blob), use(Nr);
	// we didn't find anything
	if (!m_localStorage->sql.got_data() || ind != i_ok || MK_blob.get_len()!=MK.size()) {
		m_usedDHid=0; // make sure the DHid is not set when we didn't find anything as it is later used to remove confirmed used key from DB
		return false;
	}
	// record the Nr of extracted to be able to delete it fron base later(if decrypt ends well)
	m_usedNr=Nr;

	MK_blob.read(0, (char *)(MK.data()), MK.size());
	return true;
};
/* template instanciations for Curves 25519 and 448 */
#ifdef EC25519_ENABLED
	template bool DR<C255>::session_load();
	template bool DR<C255>::session_save();
	template bool DR<C255>::trySkippedMessageKeys(const uint32_t Nr, const X<C255> &DHr, DRMKey &MK);
#endif

#ifdef EC448_ENABLED
	template bool DR<C448>::session_load();
	template bool DR<C448>::session_save();
	template bool DR<C448>::trySkippedMessageKeys(const uint32_t Nr, const X<C448> &DHr, DRMKey &MK);
#endif

/******************************************************************************/
/*                                                                            */
/*  Lime members functions                                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @brief Create a new local user based on its userId(GRUU) from table lime_LocalUsers
 *
 * use m_selfDeviceId as input, use m_X3DH_Server as input
 * populate m_db_Uid
 *
 * @return true if user was created successfully, exception is thrown otherwise.
 *
 * @exception BCTBX_EXCEPTION	thrown if user already exists in the database
 */
template <typename Curve>
bool Lime<Curve>::create_user()
{
	// check if the user is not already in the DB
	int dummy_Uid;
	m_localStorage->sql<<"SELECT Uid FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(dummy_Uid), use(m_selfDeviceId);
	if (m_localStorage->sql.got_data()) {
		throw BCTBX_EXCEPTION << "Lime user "<<m_selfDeviceId<<" cannot be created: it is already in Database - delete it before if you really want to replace it";
	}

	// generate an identity EDDSA key pair
	auto EDDSAContext = EDDSAInit<Curve>();
	bctbx_EDDSACreateKeyPair(EDDSAContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
	// store it in a blob : Public||Private
	blob Ik(m_localStorage->sql);
	Ik.write(0, (const char *)EDDSAContext->publicKey, EDDSAContext->pointCoordinateLength);
	Ik.write(EDDSAContext->pointCoordinateLength, (const char *)EDDSAContext->secretKey, EDDSAContext->secretLength);
	/* set the Ik in Lime object */
	//m_Ik = std::move(KeyPair<ED<Curve>>{EDDSAContext->publicKey, EDDSAContext->secretKey});
	bctbx_DestroyEDDSAContext(EDDSAContext);


	// insert in DB
	try {
		m_localStorage->sql<<"INSERT INTO lime_LocalUsers(UserId,Ik,server,curveId) VALUES (:userId,:Ik,:server,:curveId) ", use(m_selfDeviceId), use(Ik), use(m_X3DH_Server_URL), use(static_cast<uint8_t>(Curve::curveId()));
	} catch (exception const &e) {
		throw BCTBX_EXCEPTION << "Lime user insertion failed. DB backend says : "<<e.what();
	}
	// get the Id of inserted row
	m_localStorage->sql<<"select last_insert_rowid()",into(m_db_Uid);
	/* WARNING: previous line break portability of DB backend, specific to sqlite3.
	Following code shall work but consistently returns false and do not set m_db_Uid...*/
	/*
	if (!(m_localStorage->sql.get_last_insert_id("lime_LocalUsers", m_db_Uid)))
		throw BCTBX_EXCEPTION << "Lime user insertion failed. Couldn't retrieve last insert DB";
	}
	*/
	/* all went fine set the Ik loaded flag */
	//m_Ik_loaded = true;


	return true;
}

template <typename Curve>
void Lime<Curve>::get_SelfIdentityKey() {
	if (m_Ik_loaded == false) {
		blob Ik_blob(m_localStorage->sql);
		m_localStorage->sql<<"SELECT Ik FROM Lime_LocalUsers WHERE Uid = :UserId LIMIT 1;", into(Ik_blob), use(m_db_Uid);
		if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
			Ik_blob.read(0, (char *)(m_Ik.publicKey().data()), m_Ik.publicKey().size()); // Read the public key
			Ik_blob.read(m_Ik.publicKey().size(), (char *)(m_Ik.privateKey().data()), m_Ik.privateKey().size()); // Read the private key
			m_Ik_loaded = true; // set the flag
		}
	}
}


template <typename Curve>
void Lime<Curve>::X3DH_generate_SPk(X<Curve> &publicSPk, Signature<Curve> &SPk_sig, uint32_t &SPk_id) {
	// check Identity key is loaded in Lime object context
	get_SelfIdentityKey();

	// Generate a new ECDH Key pair
	auto ECDH_Context = ECDHInit<Curve>();
	bctbx_ECDHCreateKeyPair(ECDH_Context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);

	// Sign the public key with our identity key
	auto EDDSA_Context = EDDSAInit<Curve>();
	bctbx_EDDSA_setPublicKey(EDDSA_Context, m_Ik.publicKey().data(), m_Ik.publicKey().size());
	bctbx_EDDSA_setSecretKey(EDDSA_Context, m_Ik.privateKey().data(), m_Ik.privateKey().size());
	auto sig_size=SPk_sig.size();
	bctbx_EDDSA_sign(EDDSA_Context, ECDH_Context->selfPublic, ECDH_Context->pointCoordinateLength, nullptr, 0, SPk_sig.data(), &sig_size);

	// Generate a random SPk Id
	std::array<uint8_t,4> randomId;
	bctbx_rng_get(m_RNG, randomId.data(), randomId.size());
	SPk_id = static_cast<uint32_t>(randomId[0])<<24 | static_cast<uint32_t>(randomId[1])<<16 | static_cast<uint32_t>(randomId[2])<<8 | static_cast<uint32_t>(randomId[3]);

	// insert all this in DB
	try {
		// open a transaction as both modification shall be done or none
		transaction tr(m_localStorage->sql);

		// We must first update potential existing SPK in base from active to stale status
		m_localStorage->sql<<"UPDATE X3DH_SPK SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Uid = :Uid", use(m_db_Uid);

		blob SPk_blob(m_localStorage->sql);
		SPk_blob.write(0, (const char *)ECDH_Context->selfPublic, ECDH_Context->pointCoordinateLength);
		SPk_blob.write(ECDH_Context->pointCoordinateLength, (const char *)ECDH_Context->secret, ECDH_Context->secretLength);
		m_localStorage->sql<<"INSERT INTO X3DH_SPK(SPKid,SPK,Uid) VALUES (:SPKid,:SPK,:Uid) ", use(SPk_id), use(SPk_blob), use(m_db_Uid);

		tr.commit();
	} catch (exception const &e) {
		bctbx_DestroyECDHContext(ECDH_Context);
		bctbx_DestroyEDDSAContext(EDDSA_Context);
		throw BCTBX_EXCEPTION << "SPK insertion in DB failed. DB backend says : "<<e.what();
	}

	// get SPk public key in output param
	publicSPk = std::move(X<Curve>{ECDH_Context->selfPublic});

	// destroy contexts
	bctbx_DestroyECDHContext(ECDH_Context);
	bctbx_DestroyEDDSAContext(EDDSA_Context);
}

template <typename Curve>
void Lime<Curve>::X3DH_generate_OPks(std::vector<X<Curve>> &publicOPks, std::vector<uint32_t> &OPk_ids, const uint16_t OPk_number) {

	// make room for OPk and OPk ids
	publicOPks.reserve(OPk_number);
	OPk_ids.reserve(OPk_number);

	// Prepare DB statement
	transaction tr(m_localStorage->sql);
	blob OPk(m_localStorage->sql);
	uint32_t OPk_id;
	statement st = (m_localStorage->sql.prepare << "INSERT INTO X3DH_OPK(OPKid, OPK,Uid) VALUES(:OPKid,:OPK,:Uid)", use(OPk_id), use(OPk), use(m_db_Uid));

	// Create an ECDH context to create key pairs
	auto ECDH_Context = ECDHInit<Curve>();

	try {
		for (uint16_t i=0; i<OPk_number; i++) {
			// Generate a new ECDH Key pair
			bctbx_ECDHCreateKeyPair(ECDH_Context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);

			// Generate a random SPk Id (uint32_t)
			std::array<uint8_t,4> randomId;
			bctbx_rng_get(m_RNG, randomId.data(), randomId.size());
			OPk_id = static_cast<uint32_t>(randomId[0])<<24 | static_cast<uint32_t>(randomId[1])<<16 | static_cast<uint32_t>(randomId[2])<<8 | static_cast<uint32_t>(randomId[3]);

			// Insert in DB: store Public Key || Private Key
			OPk.write(0, (char *)(ECDH_Context->selfPublic), ECDH_Context->pointCoordinateLength);
			OPk.write(ECDH_Context->pointCoordinateLength, (char *)(ECDH_Context->secret), ECDH_Context->secretLength);
			st.execute(true);

			// set in output vectors
			publicOPks.emplace_back(ECDH_Context->selfPublic);
			OPk_ids.push_back(OPk_id);
		}
	} catch (exception &e) {
		bctbx_DestroyECDHContext(ECDH_Context);
		throw BCTBX_EXCEPTION << "OPK insertion in DB failed. DB backend says : "<<e.what();
	}
	// commit changes to DB
	tr.commit();

	bctbx_DestroyECDHContext(ECDH_Context);
}

template <typename Curve>
void Lime<Curve>::cache_DR_sessions(std::vector<recipientInfos<Curve>> &internal_recipients, std::vector<std::string> &missing_devices) {
	/* build a user list of missing ones : produce a list ready to be sent to SQL query: 'user','user','user',... also build a map to store shared_ptr to sessions */
	std::string sqlString_requestedDevices{""};
	std::unordered_map<std::string, std::shared_ptr<DR<Curve>>> requestedDevices; // found session will be loaded and temp stored in this

	size_t requestedDevicesCount = 0;
	for (auto &recipient : internal_recipients) {
		if (recipient.DRSession == nullptr) {
			sqlString_requestedDevices.append("'").append(recipient.deviceId).append("',");
			requestedDevicesCount++;
		}
	}
	if (requestedDevicesCount==0) return; // we already got them all

	sqlString_requestedDevices.pop_back(); // remove the last ','

	/* fetch them from DB */
	std::vector<long int>sessionId(requestedDevicesCount);
	std::vector<std::string>peerId(requestedDevicesCount);

	statement st = (m_localStorage->sql.prepare << "SELECT s.sessionId, d.DeviceId FROM DR_sessions as s INNER JOIN lime_PeerDevices as d ON s.Did=d.Did WHERE d.Uid= :Uid AND s.Status=1 AND d.DeviceId IN ("<<sqlString_requestedDevices<<");",  into(sessionId), into(peerId), use(m_db_Uid));

	st.execute();
	while (st.fetch()) { // we shall do it only once as we probably won't get more devices than requested, could happend if DB is in chaos and we have several sessions actives for one pair
		/* load session in cache for them */
		for (size_t i=0; i<sessionId.size(); i++ ) {
			requestedDevices[peerId[i]] = std::make_shared<DR<Curve>>(m_localStorage.get(), sessionId[i]); // load session from cache
		}

		/* loop on found sessions and store them in cache */
		for (auto &recipient : requestedDevices) {
			m_DR_sessions_cache[recipient.first] = recipient.second;
		}

		// useless but recommended by SOCI spec
		sessionId.resize(requestedDevicesCount);
		peerId.resize(requestedDevicesCount);
	}

	/* loop on internal recipient and fill it with the found ones, store the missing ones in the missing_devices vector */
	for (auto &recipient : internal_recipients) {
		if (recipient.DRSession == nullptr) { // they are missing
			auto retrievedElem = requestedDevices.find(recipient.deviceId);
			if (retrievedElem == requestedDevices.end()) { // we didn't found this one
				missing_devices.push_back(recipient.deviceId);
			} else { // we got this one
				recipient.DRSession = std::move(retrievedElem->second); // don't need this pointer in map anymore
			}
		}
	}
}

/**
 * @brief Store peer device information(DeviceId - GRUU -, public Ik, Uid to link it to a user) in local storage
 *
 * @param[in] peerDeviceId	The device id to insert
 * @param[in] Ik		The public EDDSA identity key of this device
 *
 * @return the id internally used by db to store this row
 */
template <typename Curve>
long int Lime<Curve>::store_peerDevice(const std::string &peerDeviceId, const ED<Curve> &Ik) {
	blob Ik_blob(m_localStorage->sql);

	try {
		long int Did=0;
		// make sure this device wasn't already here, if it was, check they have the same Ik
		m_localStorage->sql<<"SELECT Ik,Did FROM lime_peerDevices WHERE DeviceId = :DeviceId AND Uid = :Uid LIMIT 1;", into(Ik_blob), into(Did), use(peerDeviceId), use(m_db_Uid);
		if (m_localStorage->sql.got_data()) { // Found one
			ED<Curve> stored_Ik;
			Ik_blob.read(0, (char *)(stored_Ik.data()), stored_Ik.size()); // Read it to compare it to the given one
			if (stored_Ik == Ik) { // they match, so we just return the Did
				return Did;
			} else { // Ik are not matching, peer device changed its Ik!?! Reject
				bctbx_error("It appears that peer device %s was known with an identity key but is trying to use another one now", peerDeviceId.data());
				throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" changed its Ik";
			}
		} else { // not found in local Storage
			transaction tr(m_localStorage->sql);
			Ik_blob.write(0, (char *)(Ik.data()), Ik.size());
			m_localStorage->sql<<"INSERT INTO lime_PeerDevices(DeviceId,Uid,Ik) VALUES (:deviceId,:Uid,:Ik) ", use(peerDeviceId), use(m_db_Uid), use(Ik_blob);
			m_localStorage->sql<<"select last_insert_rowid()",into(Did);
			tr.commit();
			bctbx_debug("user %s store peerDevice %s with device id %x", m_selfDeviceId.data(), peerDeviceId.data(), Did);
			return Did;
		}
	} catch (exception const &e) {
		throw BCTBX_EXCEPTION << "Peer device "<<peerDeviceId<<" insertion failed. DB backend says : "<<e.what();
	}
}

// load from local storage in DRSessions all DR session matching the peerDeviceId, ignore the one picked by id in 2nd arg
template <typename Curve>
void Lime<Curve>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDRSessionId, std::vector<std::shared_ptr<DR<Curve>>> &DRSessions) {
	std::vector<long int> sessionIds(10); // get sessions 10 by 10, one fetch shall be enough anyway
	statement st = (m_localStorage->sql.prepare << "SELECT s.sessionId FROM DR_sessions as s INNER JOIN lime_PeerDevices as d ON s.Did=d.Did WHERE d.DeviceId = :senderDeviceId AND s.sessionId <> :ignoreThisDRSessionId ORDER BY s.Status DESC, timeStamp ASC;",  into(sessionIds), use(senderDeviceId), use(ignoreThisDRSessionId));
	st.execute();

	while (st.fetch()) {
		for (auto sessionId : sessionIds) {
			/* load session in cache DRSessions */
			DRSessions.push_back(make_shared<DR<Curve>>(m_localStorage.get(), sessionId)); // load session from cache
		}
		sessionIds.resize(10);
	}
};

/**
 * @brief retrieve matching SPk from localStorage, throw an exception if not found
 *
 * @param[in]	SPk_id	Id of the SPk we're trying to fetch
 * @param[out]	SPk	The SPk if found
 */
template <typename Curve>
void Lime<Curve>::X3DH_get_SPk(uint32_t SPk_id, KeyPair<X<Curve>> &SPk) {
	blob SPk_blob(m_localStorage->sql);
	m_localStorage->sql<<"SELECT SPk FROM X3DH_SPk WHERE Uid = :Uid AND SPKid = :SPk_id LIMIT 1;", into(SPk_blob), use(m_db_Uid), use(SPk_id);
	if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
		SPk_blob.read(0, (char *)(SPk.publicKey().data()), SPk.publicKey().size()); // Read the public key
		SPk_blob.read(SPk.publicKey().size(), (char *)(SPk.privateKey().data()), SPk.privateKey().size()); // Read the private key
	} else {
		throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<"look up for SPk id "<<SPk_id<<" failed";
	}
}


/**
 * @brief retrieve matching OPk from localStorage, throw an exception if not found
 * 	Note: once fetch, the OPk is deleted from localStorage
 *
 * @param[in]	OPk_id	Id of the OPk we're trying to fetch
 * @param[out]	OPk	The OPk if found
 */
template <typename Curve>
void Lime<Curve>::X3DH_get_OPk(uint32_t OPk_id, KeyPair<X<Curve>> &OPk) {
	blob OPk_blob(m_localStorage->sql);
	m_localStorage->sql<<"SELECT OPk FROM X3DH_OPk WHERE Uid = :Uid AND OPKid = :OPk_id LIMIT 1;", into(OPk_blob), use(m_db_Uid), use(OPk_id);
	if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
		OPk_blob.read(0, (char *)(OPk.publicKey().data()), OPk.publicKey().size()); // Read the public key
		OPk_blob.read(OPk.publicKey().size(), (char *)(OPk.privateKey().data()), OPk.privateKey().size()); // Read the private key
		m_localStorage->sql<<"DELETE FROM X3DH_OPk WHERE Uid = :Uid AND OPKid = :OPk_id;", use(m_db_Uid), use(OPk_id); // And remove it from local Storage
	} else {
		throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<"look up for OPk id "<<OPk_id<<" failed";
	}
}

/* template instanciations for Curves 25519 and 448 */
#ifdef EC25519_ENABLED
	template bool Lime<C255>::create_user();
	template void Lime<C255>::get_SelfIdentityKey();
	template void Lime<C255>::X3DH_generate_SPk(X<C255> &publicSPk, Signature<C255> &SPk_sig, uint32_t &SPk_id);
	template void Lime<C255>::X3DH_generate_OPks(std::vector<X<C255>> &publicOPks, std::vector<uint32_t> &OPk_ids, const uint16_t OPk_number);
	template void Lime<C255>::cache_DR_sessions(std::vector<recipientInfos<C255>> &internal_recipients, std::vector<std::string> &missing_devices);
	template long int Lime<C255>::store_peerDevice(const std::string &peerDeviceId, const ED<C255> &Ik);
	template void Lime<C255>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDBSessionId, std::vector<std::shared_ptr<DR<C255>>> &DRSessions);
	template void Lime<C255>::X3DH_get_SPk(uint32_t SPk_id, KeyPair<X<C255>> &SPk);
	template void Lime<C255>::X3DH_get_OPk(uint32_t OPk_id, KeyPair<X<C255>> &SPk);
#endif

#ifdef EC448_ENABLED
	template bool Lime<C448>::create_user();
	template void Lime<C448>::get_SelfIdentityKey();
	template void Lime<C448>::X3DH_generate_SPk(X<C448> &publicSPk, Signature<C448> &SPk_sig, uint32_t &SPk_id);
	template void Lime<C448>::X3DH_generate_OPks(std::vector<X<C448>> &publicOPks, std::vector<uint32_t> &OPk_ids, const uint16_t OPk_number);
	template void Lime<C448>::cache_DR_sessions(std::vector<recipientInfos<C448>> &internal_recipients, std::vector<std::string> &missing_devices);
	template long int Lime<C448>::store_peerDevice(const std::string &peerDeviceId, const ED<C448> &Ik);
	template void Lime<C448>::get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDBSessionId, std::vector<std::shared_ptr<DR<C448>>> &DRSessions);
	template void Lime<C448>::X3DH_get_SPk(uint32_t SPk_id, KeyPair<X<C448>> &SPk);
	template void Lime<C448>::X3DH_get_OPk(uint32_t OPk_id, KeyPair<X<C448>> &SPk);
#endif


} // namespace lime
