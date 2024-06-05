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

#include <soci/soci.h>

#include "lime_localStorage.hpp"
#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_impl.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "lime_x3dh_protocol.hpp"
#include "bctoolbox/exception.hh"
#include "lime_crypto_primitives.hpp"
#include <set>

using namespace::std;
using namespace::soci;
using namespace::lime;

namespace lime {
	/**
	 * @brief a X3DH engine, implements the X3DH interface.
	 *
	 * @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	class X3DHi: public X3DH {
	private:
			/* general purpose */
			std::shared_ptr<RNG> m_RNG; // Random Number Generator context
			std::string m_selfDeviceId; // self device Id, shall be the GRUU

			/* local storage related */
			std::shared_ptr<lime::Db> m_localStorage; // shared pointer would be used/stored in Double Ratchet Sessions
			long int m_db_Uid; // the Uid in database, retrieved at creation/load, used for faster access

			/* network related */
			std::string m_server_url; // url of x3dh key server
			limeX3DHServerPostData m_post_data; // externally provided function to communicate with x3dh server

			/* X3DH keys */
			DSApair<typename Curve::EC> m_Ik; // our identity key pair, is loaded from DB only if requested(to sign a SPK or to perform X3DH init)
			bool m_Ik_loaded; // did we load the Ik yet?
			void load_SelfIdentityKey(void) {
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
			};

			/**
			* @brief Generate (or load) a SPk, sign it with the given Ik.
			* The generated SPk is stored in local storage with active Status, any exiting SPk are set to inactive.
 *
			* @param[in]	Ik		Identity key used to sign the newly generated (or loaded) key
			* @param[in]	load		Flag, if set first try to load key from storage to return them and if none is found, generate it
 *
			* @return	the generated SPk with signature(private key is not set in the structure)
			*/
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			SignedPreKey<Curve> generate_SPk(const bool load=false) {
				load_SelfIdentityKey(); // make sure our Ik is loaded in object
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));

				// if the load flag is on, try to load a existing active key instead of generating it
				if (load) {
					uint32_t SPkId=0;
					blob SPk_blob(m_localStorage->sql);
					m_localStorage->sql<<"SELECT SPk, SPKid  FROM X3DH_SPk WHERE Uid = :Uid AND Status = 1 LIMIT 1;", into(SPk_blob), into(SPkId), use(m_db_Uid);
					if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
						sBuffer<SignedPreKey<Curve>::serializedSize()> serializedSPk{};
						SPk_blob.read(0, (char *)(serializedSPk.data()), SignedPreKey<Curve>::serializedSize());
						SignedPreKey<Curve> s(serializedSPk, SPkId);
						// Sign the public key with our identity key
						auto SPkSign = make_Signature<Curve>();
						SPkSign->set_public(m_Ik.cpublicKey());
						SPkSign->set_secret(m_Ik.cprivateKey());
						SPkSign->sign(s.serializePublic(true), s.signature());
						return s;
					}
				}
					// Generate a new ECDH Key pair
				auto DH = make_keyExchange<Curve>();
				DH->createKeyPair(m_RNG);
				SignedPreKey<Curve> s(DH->get_selfPublic(), DH->get_secret());

				// Sign the public key with our identity key
				auto SPkSign = make_Signature<Curve>();
				SPkSign->set_public(m_Ik.cpublicKey());
				SPkSign->set_secret(m_Ik.cprivateKey());
				SPkSign->sign(s.serializePublic(true), s.signature());

				// Generate a random SPk Id
				// Sqlite doesn't really support unsigned value, the randomize function makes sure that the MSbit is set to 0 to not fall into strange bugs with that
				// SPkIds must be random but unique, get one not already in
				std::set<uint32_t> activeSPkIds{};
				// fetch existing SPk ids from DB (SPKid is unique on all users, so really get them all, do not restrict with m_db_Uid)
				rowset<row> rs = (m_localStorage->sql.prepare << "SELECT SPKid FROM X3DH_SPK");
				for (const auto &r : rs) {
					auto activeSPkId = static_cast<uint32_t>(r.get<int>(0));
					activeSPkIds.insert(activeSPkId);
				}

				uint32_t SPkId = m_RNG->randomize();
				while (activeSPkIds.insert(SPkId).second == false) { // This one was already in
					SPkId = m_RNG->randomize();
				}
				s.set_Id(SPkId);

				// insert all this in DB
				try {
					// open a transaction as both modification shall be done or none
					transaction tr(m_localStorage->sql);

					// We must first update potential existing SPK in base from active to stale status
					m_localStorage->sql<<"UPDATE X3DH_SPK SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Uid = :Uid AND Status = 1;", use(m_db_Uid);

					blob SPk_blob(m_localStorage->sql);
					SPk_blob.write(0, (const char *)s.serialize().data(),  SignedPreKey<Curve>::serializedSize());
					m_localStorage->sql<<"INSERT INTO X3DH_SPK(SPKid,SPK,Uid) VALUES (:SPKid,:SPK,:Uid) ", use(SPkId), use(SPk_blob), use(m_db_Uid);

					tr.commit();
				} catch (exception const &e) {
					throw BCTBX_EXCEPTION << "SPK insertion in DB failed. DB backend says : "<<e.what();
				}
				return s;
			}
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			SignedPreKey<Curve> generate_SPk(const bool load=false) {
				load_SelfIdentityKey(); // make sure our Ik is loaded in object
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));

				// if the load flag is on, try to load a existing active key instead of generating it
				if (load) {
					uint32_t SPkId=0;
					blob SPk_blob(m_localStorage->sql);
					m_localStorage->sql<<"SELECT SPk, SPKid  FROM X3DH_SPk WHERE Uid = :Uid AND Status = 1 LIMIT 1;", into(SPk_blob), into(SPkId), use(m_db_Uid);
					if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
						sBuffer<SignedPreKey<Curve>::serializedSize()> serializedSPk{};
						SPk_blob.read(0, (char *)(serializedSPk.data()), SignedPreKey<Curve>::serializedSize());
						SignedPreKey<Curve> s(serializedSPk, SPkId);
						// Sign the public key with our identity key
						auto SPkSign = make_Signature<typename Curve::EC>();
						SPkSign->set_public(m_Ik.cpublicKey());
						SPkSign->set_secret(m_Ik.cprivateKey());
						SPkSign->sign(s.serializePublic(true), s.signature());
						return s;
					}
				}
				// Generate a new ECDH Key pair
				auto DH = make_keyExchange<typename Curve::EC>();
				DH->createKeyPair(m_RNG);
				// Generate a new KEM Key pair
				auto KEMengine = make_KEM<typename Curve::KEM>();
				Kpair<typename Curve::KEM> kemSPk{};
				KEMengine->createKeyPair(kemSPk);
				SignedPreKey<Curve> s(DH->get_selfPublic(), DH->get_secret(), kemSPk.cpublicKey(), kemSPk.cprivateKey());

				// Sign the public key with our identity key
				auto SPkSign = make_Signature<typename Curve::EC>();
				SPkSign->set_public(m_Ik.cpublicKey());
				SPkSign->set_secret(m_Ik.cprivateKey());
				SPkSign->sign(s.serializePublic(true), s.signature());

				// Generate a random SPk Id
				// Sqlite doesn't really support unsigned value, the randomize function makes sure that the MSbit is set to 0 to not fall into strange bugs with that
				// SPkIds must be random but unique, get one not already in
				std::set<uint32_t> activeSPkIds{};
				// fetch existing SPk ids from DB (SPKid is unique on all users, so really get them all, do not restrict with m_db_Uid)
				rowset<row> rs = (m_localStorage->sql.prepare << "SELECT SPKid FROM X3DH_SPK");
				for (const auto &r : rs) {
					auto activeSPkId = static_cast<uint32_t>(r.get<int>(0));
					activeSPkIds.insert(activeSPkId);
				}

				uint32_t SPkId = m_RNG->randomize();
				while (activeSPkIds.insert(SPkId).second == false) { // This one was already in
					SPkId = m_RNG->randomize();
				}
				s.set_Id(SPkId);

				// insert all this in DB
				try {
					// open a transaction as both modification shall be done or none
					transaction tr(m_localStorage->sql);

					// We must first update potential existing SPK in base from active to stale status
					m_localStorage->sql<<"UPDATE X3DH_SPK SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Uid = :Uid AND Status = 1;", use(m_db_Uid);

					blob SPk_blob(m_localStorage->sql);
					SPk_blob.write(0, (const char *)s.serialize().data(),  SignedPreKey<Curve>::serializedSize());
					m_localStorage->sql<<"INSERT INTO X3DH_SPK(SPKid,SPK,Uid) VALUES (:SPKid,:SPK,:Uid) ", use(SPkId), use(SPk_blob), use(m_db_Uid);

					tr.commit();
				} catch (exception const &e) {
					throw BCTBX_EXCEPTION << "SPK insertion in DB failed. DB backend says : "<<e.what();
				}
				return s;
			}

			/**
			* @brief Generate (or load) a batch of OPks, store them in local storage and return their public keys with their ids.
			*
			* @param[out]	OPks		A vector of all the generated (or loaded) OPks
			* @param[in]	OPk_number	How many keys shall we generate. This parameter is ignored if the load flag is set and we find some keys to load
			* @param[in]	load		Flag, if set first try to load keys from storage to return them and if none found just generate the requested amount
			*/
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void generate_OPks(std::vector<OneTimePreKey<Curve>> &OPks, const uint16_t OPk_number, const bool load=false) {

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
			/**
			 * EC/KEM version of OPk generation: we must also sign the OPk
			 */
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void generate_OPks(std::vector<OneTimePreKey<Curve>> &OPks, const uint16_t OPk_number, const bool load=false) {

				load_SelfIdentityKey(); // make sure our Ik is loaded in object
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
					// Get Keys matching the current user and that are not set as dispatched yet (Status = 1)
					statement st = (m_localStorage->sql.prepare << "SELECT OPk FROM X3DH_OPK WHERE Uid = :Uid AND Status = 1 AND OPKid = :OPkId;", into(OPk_blob), use(m_db_Uid), use(OPk_id));

					for (uint32_t id : activeOPkIds) { // We already have all the active OPK ids, loop on them
						OPk_id = id; // copy the id into the bind variable
						st.execute(true);
						if (m_localStorage->sql.got_data()) {
							sBuffer<OneTimePreKey<Curve>::serializedSize()> serializedOPk{};
							OPk_blob.read(0, (char *)(serializedOPk.data()), OneTimePreKey<Curve>::serializedSize());
							OneTimePreKey<Curve> OPk(serializedOPk, OPk_id);
							OPks.push_back(OPk);
						}
					}

					if (OPks.size()>0) { // We found some OPks, all set then
						return;
					}
				}

				// we must create OPk_number new OPks
				// Create an key exchange context to create key pairs
				auto DH = make_keyExchange<typename Curve::EC>();
				auto KEMengine = make_KEM<typename Curve::KEM>();
				while (OPks.size() < OPk_number){
					// Generate a random OPk Id
					// Sqlite doesn't really support unsigned value, the randomize function makes sure that the MSbit is set to 0 to not fall into strange bugs with that
					uint32_t OPk_id = m_RNG->randomize();

					if (activeOPkIds.insert(OPk_id).second) { // if this Id wasn't in the set, use it
						// Generate a new ECDH Key pair
						DH->createKeyPair(m_RNG);
						// Generate a new KEM Key pair
						Kpair<typename Curve::KEM> kemOPk{};
						KEMengine->createKeyPair(kemOPk);
						OneTimePreKey<Curve> OPk(DH->get_selfPublic(), DH->get_secret(), kemOPk.cpublicKey(), kemOPk.cprivateKey(), OPk_id);
						OPks.push_back(OPk);
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

			/**
			* @brief update OPk Status so we can get an idea of what's on server and what was dispatched but not used yet
			* 	get rid of anyone with status 0 and oldest than OPk_limboTime_days
			*
			* @param[in]	OPkIds	List of Ids found on server
			*/
			void updateOPkStatus(const std::vector<uint32_t> &OPkIds) {
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

			/**
			* @brief Create a new local user based on its userId(GRUU) from table lime_LocalUsers
			* The user will be activated only after being published successfully on X3DH server
			*
			* use m_selfDeviceId as input
			* populate m_db_Uid
			*
			* @exception BCTBX_EXCEPTION	thrown if user is not found in base
			*/
			void activate_user(void) {
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
			}

			/**
			* @brief Clean user data in case of problem or when we're done, it also processes the asynchronous encryption queue
			*
			* @param[in,out] userData	the structure holding the data structure captured by the process response lambda
			*/
			void cleanUserData(std::shared_ptr<Lime<Curve>> limeObj, std::shared_ptr<callbackUserData> userData) {
				if (userData->plainMessage!=nullptr) { // only encryption request for X3DH bundle would populate the plainMessage field of user data structure
					limeObj->processEncryptionQueue();
				} else { // its not an encryption, just set userData to null it shall destroy it
					userData = nullptr;
				}
			}

			/**
			* @brief process response message from X3DH server
			*
			* @param[in]		limeObj		The lime object linked to this reponse, also found in the userData but already checked and casted back to the type we need
			* @param[in,out]	userData	the structure holding the data structure associated to the current asynchronous operation
			* @param[in]		reponseCode	response from X3DH server, communication is done over HTTP(S), so we expect a 200
			* 					other code will just lead to cleaning memory
			* @param[in]		responseBody	a vector holding the actual response from server to be processed
			*/
			void process_response(std::shared_ptr<Lime<Curve>> limeObj, std::shared_ptr<callbackUserData> userData, int responseCode, const std::vector<uint8_t> &responseBody) {
				auto callback = userData->callback; // get callback

				if (responseCode == 200) { // HTTP server is happy with our packet
					// check response from X3DH server: header shall be X3DH protocol version || message type || curveId
					lime::x3dh_protocol::x3dh_message_type message_type{x3dh_protocol::x3dh_message_type::error}; // initialise to error type, shall be overridden by the parseMessage_getType function
					lime::x3dh_protocol::x3dh_error_code error_code{x3dh_protocol::x3dh_error_code::unset_error_code};

					// check message validity, extract type and error code(if any)
					LIME_LOGI<<"Parse incoming X3DH message for user "<< this->m_selfDeviceId;
					if (!x3dh_protocol::parseMessage_getType<Curve>(responseBody, message_type, error_code, callback)) {
						cleanUserData(limeObj, userData);
						return;
					}

					switch (message_type) {
						case x3dh_protocol::x3dh_message_type::registerUser: {
							// server response to a registerUser
							// activate the local user
							try {
								activate_user();
							} catch (BctbxException const &e) {
								LIME_LOGE<<"Cannot activate user "<< m_selfDeviceId << ". Backend says: "<< e.str();
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Cannot activate user : "}.append(e.str()));
								cleanUserData(limeObj, userData);
								return;
							} catch (exception const &e) { // catch all and let flow it up
								LIME_LOGE<<"Cannot activate user "<< m_selfDeviceId << ". Backend says: "<< e.what();
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Cannot activate user : "}.append(e.what()));
								cleanUserData(limeObj, userData);
								return;
							}
						}
						break;

						case x3dh_protocol::x3dh_message_type::postSPk:
						case x3dh_protocol::x3dh_message_type::deleteUser:
						case x3dh_protocol::x3dh_message_type::postOPks:
							// server response to deleteUser, postSPk or postOPks, nothing to do really
							// success callback is the common behavior, performed after the switch
						break;

						case x3dh_protocol::x3dh_message_type::peerBundle: {
							// server response to a getPeerBundle packet
							std::vector<X3DH_peerBundle<Curve>> peersBundle;
							if (!x3dh_protocol::parseMessage_getPeerBundles(responseBody, peersBundle)) { // parsing went wrong
								LIME_LOGE<<"Got an invalid peerBundle packet from X3DH server";
								if (callback) callback(lime::CallbackReturn::fail, "Got an invalid peerBundle packet from X3DH server");
								cleanUserData(limeObj, userData);
								return;
							}

							// generate X3DH init packets, create a store DR Sessions(in Lime obj cache, they'll be stored in DB when the first encryption will occurs)
							try {
								//Note: if while we were waiting for the peer bundle we did get an init message from him and created a session
								// just do nothing : create a second session with the peer bundle we retrieved and at some point one session will stale
								// when message stop crossing themselves on the network
								init_sender_session(limeObj, peersBundle);
							} catch (BctbxException &e) { // something went wrong, go for callback as this function may be called by code not supporting exceptions
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Error during the peer Bundle processing : "}.append(e.str()));
								cleanUserData(limeObj, userData);
								return;
							} catch (exception const &e) {
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Error during the peer Bundle processing : "}.append(e.what()));
								cleanUserData(limeObj, userData);
								return;
							}

							// tweak the userData->recipients to set to fail those wo didn't get a key bundle
							for (const auto &peerBundle:peersBundle) {
								// get all the bundless peer Devices
								if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
									for (auto &recipient:*(userData->recipients)) {
										// and set their recipient status to fail so the encrypt function would ignore them
										if (recipient.deviceId == peerBundle.deviceId) {
											recipient.peerStatus = lime::PeerDeviceStatus::fail;
										}
									}
								}
							}

							// call the encrypt function again, it will call the callback when done, encryption queue won't be processed as still locked by the m_ongoing_encryption member
							// We must not generate an exception here, so catch anything raising from encrypt
							try {
								limeObj->encrypt(userData->recipientUserId, userData->recipients, userData->plainMessage, userData->encryptionPolicy, userData->cipherMessage, callback);
							} catch (BctbxException &e) { // something went wrong, go for callback as this function may be called by code not supporting exceptions
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Error during the encryption after the peer Bundle processing : "}.append(e.str()));
								cleanUserData(limeObj, userData);
								return;
							} catch (exception const &e) {
								if (callback) callback(lime::CallbackReturn::fail, std::string{"Error during the encryption after the peer Bundle processing : "}.append(e.what()));
								cleanUserData(limeObj, userData);
								return;
							}

							// now we can safely delete the user data, note that this may trigger an other encryption if there is one in queue
							cleanUserData(limeObj, userData);
						}
						return;

						case x3dh_protocol::x3dh_message_type::selfOPks: {
							// server response to a getSelfOPks
							std::vector<uint32_t> selfOPkIds{};
							if (!x3dh_protocol::parseMessage_selfOPks<Curve>(responseBody, selfOPkIds)) { // parsing went wrong
								LIME_LOGE<<"Got an invalid selfOPKs packet from X3DH server";
								if (callback) callback(lime::CallbackReturn::fail, "Got an invalid selfOPKs packet from X3DH server");
								cleanUserData(limeObj, userData);
								return;
							}

							// update in LocalStorage the OPk status: tag removed from server and delete old keys
							updateOPkStatus(selfOPkIds);

							// Check if we shall upload more packets
							if (selfOPkIds.size() < userData->OPkServerLowLimit) {
								// generate and publish the OPks
								std::vector<OneTimePreKey<Curve>> OPks{};
								// Generate OPks OPkBatchSize (or more if we need more to reach ServerLowLimit)
								generate_OPks(OPks, std::max(userData->OPkBatchSize, static_cast<uint16_t>(userData->OPkServerLowLimit - selfOPkIds.size())) );
								std::vector<uint8_t> X3DHmessage{};
								x3dh_protocol::buildMessage_publishOPks(X3DHmessage, OPks);
								postToX3DHServer(userData, X3DHmessage);
							} else { /* nothing to do, just call the callback */
								if (callback) callback(lime::CallbackReturn::success, "");
								cleanUserData(limeObj, userData);
							}
						}
						return;

						case x3dh_protocol::x3dh_message_type::error: {
							// error messages are logged inside the parseMessage_getType function, just return failure to callback
							// Check if the error message is a user_not_found and we were trying to get our self OPks(OPkServerLowLimit > 0)
							if (error_code == lime::x3dh_protocol::x3dh_error_code::user_not_found && userData->OPkServerLowLimit > 0) {
								// We must republish the user, something went terribly wrong on server side and we're not there anymore
								LIME_LOGW<<"Something went terribly wrong on server "<<m_server_url<<". Republish user "<<m_selfDeviceId;
								updateOPkStatus(std::vector<uint32_t>{}); // set all OPks to dispatched status as we don't know if some of them where dispatched or not
								// republish the user, it will keep same Ik and SPk but generate new OPks as we just set all our OPk to dispatched
								publish_user(userData, userData->OPkServerLowLimit);
								cleanUserData(limeObj, userData);
							} else {
								if (callback) callback(lime::CallbackReturn::fail, "X3DH server error");
								cleanUserData(limeObj, userData);
							}
						}
						return;

						// for registerUser, deleteUser, postSPk and postOPks, on success, server will respond with an identical header
						// but we cannot get from server getPeerBundle or getSelfOPks message
						case x3dh_protocol::x3dh_message_type::deprecated_registerUser:
						case x3dh_protocol::x3dh_message_type::getPeerBundle:
						case x3dh_protocol::x3dh_message_type::getSelfOPks: {
							if (callback) callback(lime::CallbackReturn::fail, "X3DH unexpected message from server");
							cleanUserData(limeObj, userData);
						}
						return;

					}

					// we get here only if processing is over and response was the expected one
					if (callback) callback(lime::CallbackReturn::success, "");
					cleanUserData(limeObj, userData);
					return;

				} else { // response code is not 200Ok
					if (callback) callback(lime::CallbackReturn::fail, std::string("Got a non Ok response from server : ").append(std::to_string(responseCode)));
					cleanUserData(limeObj, userData);
					return;
				}
			}

			/**
			* @brief retrieve matching SPk from localStorage, throw an exception if not found
			*
			* @param[in]	SPk_id	Id of the SPk we're trying to fetch
			* @return 	The SPk if found
			*/
			SignedPreKey<Curve> get_SPk(uint32_t SPk_id) {
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
				blob SPk_blob(m_localStorage->sql);
				m_localStorage->sql<<"SELECT SPk FROM X3DH_SPk WHERE Uid = :Uid AND SPKid = :SPk_id LIMIT 1;", into(SPk_blob), use(m_db_Uid), use(SPk_id);
				if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
					sBuffer<SignedPreKey<Curve>::serializedSize()> serializedSPk{};
					SPk_blob.read(0, (char *)(serializedSPk.data()), SignedPreKey<Curve>::serializedSize());
					return SignedPreKey<Curve>(serializedSPk, SPk_id);
				} else {
					throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<" look up for SPk id "<<std::hex<<SPk_id<<" failed";
				}
			}

			/**
			* @brief retrieve matching OPk from localStorage, throw an exception if not found
			* 	Note: once fetch, the OPk is deleted from localStorage
			*
			* @param[in]	OPk_id	Id of the OPk we're trying to fetch
			* @return The OPk if found
			*/
			OneTimePreKey<Curve> get_OPk(uint32_t OPk_id) {
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
				blob OPk_blob(m_localStorage->sql);
				m_localStorage->sql<<"SELECT OPk FROM X3DH_OPK WHERE Uid = :Uid AND OPKid = :OPk_id LIMIT 1;", into(OPk_blob), use(m_db_Uid), use(OPk_id);
				if (m_localStorage->sql.got_data()) { // Found it, it is stored in one buffer Public || Private
					sBuffer<OneTimePreKey<Curve>::serializedSize()> serializedOPk{};
					OPk_blob.read(0, (char *)(serializedOPk.data()), OneTimePreKey<Curve>::serializedSize());
					return OneTimePreKey<Curve>(serializedOPk, OPk_id);
				} else {
					throw BCTBX_EXCEPTION << "X3DH "<<m_selfDeviceId<<" look up for OPk id "<<std::hex<<OPk_id<<" failed";
				}
			}

			/**
			* @brief send a message to X3DH server
			*
			* 	this function also binds the response processing to the process_response function capturing the given userData structure
			*
			* @param[in,out]	userData	the structure holding the data structure associated to the current asynchronous operation
			* @param[in]		message		the message to be sent
			*/
			void postToX3DHServer(std::shared_ptr<callbackUserData> userData, const std::vector<uint8_t> &message) {
				LIME_LOGI<<"Post outgoing X3DH message from user "<<this->m_selfDeviceId;

				// copy capture the shared_ptr to userData
				m_post_data(m_server_url, m_selfDeviceId, message, [userData](int responseCode, const std::vector<uint8_t> &responseBody) {
						auto thiz = userData->limeObj.lock(); // get a shared pointer to Lime Object from the weak pointer stored in userData
						// check it is valid (lock() returns nullptr)
						if (!thiz) { // our Lime caller object doesn't exists anymore
							LIME_LOGE<<"Got response from X3DH server but our Lime Object has been destroyed";
							return; // the captured shared_ptr on userData will be freed when this capture will be destroyed
						}
						auto that = std::dynamic_pointer_cast<Lime<Curve>>(thiz);
						auto X3DHengine = std::dynamic_pointer_cast<X3DHi<Curve>>(that->get_X3DH());
						X3DHengine->process_response(that, userData, responseCode, responseBody);
					});
			}

			/**
			* @brief Get a vector of peer bundle and initiate a DR Session with it. Created sessions are stored in lime cache and db along the X3DH init packet
			*  as decribed in X3DH reference section 3.3
			*/
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void init_sender_session(std::shared_ptr<Lime<Curve>> limeObj, const std::vector<X3DH_peerBundle<Curve>> &peersBundle) {
				load_SelfIdentityKey(); // make sure Ik is in context
				for (const auto &peerBundle : peersBundle) {
					// do we have a key bundle to build this message from ?
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
						continue;
					}
					// Verifify SPk_signature, throw an exception if it fails
					auto SPkVerify = make_Signature<Curve>();
					SPkVerify->set_public(peerBundle.Ik);

					if (!SPkVerify->verify(peerBundle.SPk.serializePublic(true), peerBundle.SPk.csignature())) {
						LIME_LOGE<<"X3DH: SPk signature verification failed for device "<<peerBundle.deviceId;
						throw BCTBX_EXCEPTION << "Verify signature on SPk failed for deviceId "<<peerBundle.deviceId;
					}

					// before going on, check if peer informations are ok, if the returned Id is 0, it means this peer was not in storage yet
					// throw an exception in case of failure, just let it flow up
					auto peerDid = m_localStorage->check_peerDevice(peerBundle.deviceId, peerBundle.Ik);

					// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F
					// use sBuffer of size able to hold also DH4 even if we may not use it
					sBuffer<DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4> HKDF_input;
					HKDF_input.fill(0xFF); // HKDF_input holds F
					size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

					// Compute DH1 = DH(self Ik, peer SPk) - selfIk context already holds selfIk.
					auto DH = make_keyExchange<Curve>();
					DH->set_secret(m_Ik.privateKey()); // Ik Signature key is converted to keyExchange format
					DH->set_selfPublic(m_Ik.publicKey());
					DH->set_peerPublic(peerBundle.SPk.cpublicKey());
					DH->computeSharedSecret();
					auto DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
					HKDF_input_index += DH_out.size();

					// Generate Ephemeral key Exchange key pair: Ek, from now DH will hold Ek as private and self public key
					DH->createKeyPair(m_RNG);

					// Compute DH3 = DH(Ek, peer SPk) - peer SPk was already set as peer Public
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

					// Compute DH2 = DH(Ek, peer Ik)
					DH->set_peerPublic(peerBundle.Ik); // peer Ik Signature key is converted to keyExchange format
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
					HKDF_input_index += 2*DH_out.size();

					// Compute DH4 = DH(Ek, peer OPk) (if any OPk in bundle)
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
						DH->set_peerPublic(peerBundle.OPk.cpublicKey());
						DH->computeSharedSecret();
						DH_out = DH->get_sharedSecret();
						std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
						HKDF_input_index += DH_out.size();
					}

					// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4)
					DRChainKey SK;
					/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
					std::vector<uint8_t> salt(SHA512::ssize(), 0);
					HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, lime::settings::X3DH_SK_info.data(), lime::settings::X3DH_SK_info.size(), SK.data(), SK.size());

					// Generate X3DH init message: as in X3DH spec section 3.3:
					std::vector<uint8_t> X3DH_initMessage{};
					double_ratchet_protocol::buildMessage_X3DHinit(X3DH_initMessage, m_Ik.publicKey(), DH->get_selfPublic(), peerBundle.SPk.get_Id(), peerBundle.OPk.get_Id(), (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk));

					DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

					// Generate the shared AD used in DR session
					SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
					std::vector<uint8_t>AD_input{m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend()};
					AD_input.insert(AD_input.end(), peerBundle.Ik.cbegin(), peerBundle.Ik.cend());
					AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
					AD_input.insert(AD_input.end(), peerBundle.deviceId.cbegin(), peerBundle.deviceId.cend());
					HMAC_KDF<SHA512>(salt.data(), salt.size(), AD_input.data(), AD_input.size(), lime::settings::X3DH_AD_info.data(), lime::settings::X3DH_AD_info.size(), AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

					// Generate DR_Session and put it in cache(but not in localStorage yet, that would be done when first message generation will be complete)
					// it could happend that we eventually already have a session for this peer device if we received an initial message from it while fetching its key bundle(very unlikely but...)
					// in that case just keep on building our new session so the peer device knows it must get rid of the OPk, sessions will eventually converge into only one when messages
					// stop crossing themselves on the network.
					// If the fetch bundle doesn't hold OPk, just ignore our newly built session, and use existing one
					auto lock = limeObj->lock(); // get lock on the lime Obj before modifying the DR cache
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
						limeObj->DRcache_delete(peerBundle.deviceId); // will just do nothing if this peerDeviceId is not in cache
					}

					limeObj->DRcache_insert(peerBundle.deviceId, std::static_pointer_cast<DR>(make_DR_for_sender<Curve>(m_localStorage, SK, AD, peerBundle.SPk, peerDid, peerBundle.deviceId, peerBundle.Ik, m_db_Uid, X3DH_initMessage, m_RNG))); // will just do nothing if this peerDeviceId is already in cache

					LIME_LOGI<<"X3DH created session with device "<<peerBundle.deviceId;
				}
			}
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void init_sender_session(std::shared_ptr<Lime<Curve>> limeObj, const std::vector<X3DH_peerBundle<Curve>> &peersBundle) {

				load_SelfIdentityKey(); // make sure Ik is in context
				for (const auto &peerBundle : peersBundle) {
					// do we have a key bundle to build this message from ?
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
						continue;
					}
					// Verifify SPk_signature, throw an exception if it fails
					auto peerIkVerify = make_Signature<typename Curve::EC>();
					peerIkVerify->set_public(peerBundle.Ik);

					if (!peerIkVerify->verify(peerBundle.SPk.serializePublic(true), peerBundle.SPk.csignature())) {
						LIME_LOGE<<"X3DH: SPk signature verification failed for device "<<peerBundle.deviceId;
						throw BCTBX_EXCEPTION << "Verify signature on SPk failed for deviceId "<<peerBundle.deviceId;
					}

					// before going on, check if peer informations are ok, if the returned Id is 0, it means this peer was not in storage yet
					// throw an exception in case of failure, just let it flow up
					auto peerDid = m_localStorage->check_peerDevice(peerBundle.deviceId, peerBundle.Ik);

					// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F
					// The KEM augmented version will also encapsulate a secret for the given KEM PK in OPk - or SPk when no OPk is given
					// The derivation also include a transcript of all public key used: IkA || EkA || IkB || SPkB || OPkB(if present) || KEM-Key (OPk or SPk if PQ-OPk is not present) || KEM Cipher text
					// use sBuffer of size able to hold the data when OPk is present even if we may not use it
					sBuffer<
					DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4 + K<Curve, lime::Ktype::sharedSecret>::ssize() // secrets
					+ DSA<Curve, lime::DSAtype::publicKey>::ssize() +  X<Curve, lime::Xtype::sharedSecret>::ssize() // IkA || EkA
					+ DSA<Curve, lime::DSAtype::publicKey>::ssize() +  X<Curve, lime::Xtype::sharedSecret>::ssize()*2 // IkB || SPkB || OPkB
					+ K<Curve, lime::Ktype::publicKey>::ssize() + K<Curve, lime::Ktype::cipherText>::ssize() // KEM-OPk or KEM-SPk || Kem cipher text
					> HKDF_input;
					HKDF_input.fill(0xFF); // HKDF_input holds F
					size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

					// Compute DH1 = DH(self Ik, peer SPk) - selfIk context already holds selfIk.
					auto DH = make_keyExchange<typename Curve::EC>();
					DH->set_secret(m_Ik.privateKey()); // Ik Signature key is converted to keyExchange format
					DH->set_selfPublic(m_Ik.publicKey());
					DH->set_peerPublic(peerBundle.SPk.cECpublicKey());
					DH->computeSharedSecret();
					auto DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
					HKDF_input_index += DH_out.size();

					// Generate Ephemeral key Exchange key pair: Ek, from now DH will hold Ek as private and self public key
					DH->createKeyPair(m_RNG);

					// Compute DH3 = DH(Ek, peer SPk) - peer SPk was already set as peer Public
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

					// Compute DH2 = DH(Ek, peer Ik)
					DH->set_peerPublic(peerBundle.Ik); // peer Ik Signature key is converted to keyExchange format
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
					HKDF_input_index += 2*DH_out.size();

					// Compute DH4 = DH(Ek, peer OPk) (if any OPk in bundle)
					// Compute KEM1 = encaps(peer OPk) KEM1 = encaps(peer SPk) when no OPk is present
					auto KEMengine = make_KEM<typename Curve::KEM>();
					K<typename Curve::KEM, lime::Ktype::cipherText> cipherText{};
					K<typename Curve::KEM, lime::Ktype::sharedSecret> sharedSecret{};
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
						DH->set_peerPublic(peerBundle.OPk.cECpublicKey());
						DH->computeSharedSecret();
						DH_out = DH->get_sharedSecret();
						std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
						HKDF_input_index += DH_out.size();
						KEMengine->encaps(peerBundle.OPk.cKEMpublicKey(), cipherText, sharedSecret);
					} else { // There is no OPk, encapsulate a secret for the kem SPk
						KEMengine->encaps(peerBundle.SPk.cKEMpublicKey(), cipherText, sharedSecret);
					}
					std::copy_n(sharedSecret.cbegin(), sharedSecret.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4 || KEM1
					HKDF_input_index += sharedSecret.size();

					// Append the transcript to the HKDF input: IkA || EkA || IkB || EC-SPkB || [EC-OPkB || KEM-OPkB] OR [KEM-SPkB] || KEM cipherText
					std::copy_n(m_Ik.publicKey().cbegin(), m_Ik.publicKey().size(), HKDF_input.begin()+HKDF_input_index); // IkA
					HKDF_input_index += m_Ik.publicKey().size();
					std::copy_n(DH->get_selfPublic().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), HKDF_input.begin()+HKDF_input_index); // EkA
					HKDF_input_index += X<Curve, lime::Xtype::publicKey>::ssize();
					std::copy_n(peerBundle.Ik.cbegin(), peerBundle.Ik.size(), HKDF_input.begin()+HKDF_input_index); // IkB
					HKDF_input_index += peerBundle.Ik.size();
					std::copy_n(peerBundle.SPk.cECpublicKey().cbegin(), peerBundle.SPk.cECpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // EC-SPkB
					HKDF_input_index += peerBundle.SPk.cECpublicKey().size();
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
						std::copy_n(peerBundle.OPk.cECpublicKey().cbegin(), peerBundle.OPk.cECpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // EC-OPkB
						HKDF_input_index += peerBundle.OPk.cECpublicKey().size();
						std::copy_n(peerBundle.OPk.cKEMpublicKey().cbegin(), peerBundle.OPk.cKEMpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // KEM-OPkB
						HKDF_input_index += peerBundle.OPk.cKEMpublicKey().size();
					} else {
						std::copy_n(peerBundle.SPk.cKEMpublicKey().cbegin(), peerBundle.SPk.cKEMpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // KEM-SPkB
						HKDF_input_index += peerBundle.SPk.cKEMpublicKey().size();
					}
					std::copy_n(cipherText.cbegin(), cipherText.size(), HKDF_input.begin()+HKDF_input_index); // KEM-cipherText
					HKDF_input_index += cipherText.size();


					// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4 || KEM1 || transcript)
					DRChainKey SK;
					/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
					/* the info label as specified in PQXDH section 2.2 : <Identifier>_<EC algo Id>_<hash algo id>_<kem algo id> */
					std::vector<uint8_t> salt(SHA512::ssize(), 0);
					std::string HKDF_info(lime::settings::X3DH_SK_info);
					HKDF_info.append("_").append(Curve::EC::Id()).append("_SHA512_").append(Curve::KEM::Id());
					HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, HKDF_info.data(), HKDF_info.size(), SK.data(), SK.size());

					// Generate X3DH init message: as in X3DH spec section 3.3:
					std::vector<uint8_t> X3DH_initMessage{};
					double_ratchet_protocol::buildMessage_X3DHinit<Curve>(X3DH_initMessage, m_Ik.publicKey(), DH->get_selfPublic(), cipherText, peerBundle.SPk.get_Id(), peerBundle.OPk.get_Id(), (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk));

					DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

					// Generate the shared AD used in DR session
					SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
					std::vector<uint8_t>AD_input{m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend()};
					AD_input.insert(AD_input.end(), peerBundle.Ik.cbegin(), peerBundle.Ik.cend());
					AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
					AD_input.insert(AD_input.end(), peerBundle.deviceId.cbegin(), peerBundle.deviceId.cend());
					HMAC_KDF<SHA512>(salt.data(), salt.size(), AD_input.data(), AD_input.size(), lime::settings::X3DH_AD_info.data(), lime::settings::X3DH_AD_info.size(), AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

					// Generate DR_Session and put it in cache(but not in localStorage yet, that would be done when first message generation will be complete)
					// it could happend that we eventually already have a session for this peer device if we received an initial message from it while fetching its key bundle(very unlikely but...)
					// in that case just keep on building our new session so the peer device knows it must get rid of the OPk, sessions will eventually converge into only one when messages
					// stop crossing themselves on the network.
					// If the fetch bundle doesn't hold OPk, just ignore our newly built session, and use existing one
					auto lock = limeObj->lock(); // get lock on the lime Obj before modifying the DR cache
					if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
						limeObj->DRcache_delete(peerBundle.deviceId); // will just do nothing if this peerDeviceId is not in cache
					}

					limeObj->DRcache_insert(peerBundle.deviceId, std::static_pointer_cast<DR>(make_DR_for_sender<Curve>(m_localStorage, SK, AD, peerBundle.SPk, peerDid, peerBundle.deviceId, peerBundle.Ik, m_db_Uid, X3DH_initMessage, m_RNG))); // will just do nothing if this peerDeviceId is already in cache

					LIME_LOGI<<"X3DH created session with device "<<peerBundle.deviceId;
				}
			}

			/**
			 * Execute the X3DH protocol on receiver side
			 *
			 * @param[in]	X3DH_initMessage	X3DH init message buffer
			 * @param[out]	peerIk				peer's public identity key
			 * @param[out]	SPk					Self SPk
			 * @param[out]	OPk_id				the OPk id to use, 0 if no OPk are used
			 *
			 * @return the shared secret generated by the X3DH exchange
			 */
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			DRChainKey X3DH_receiver(const std::vector<uint8_t> X3DH_initMessage, DSA<Curve, lime::DSAtype::publicKey> &peerIk, SignedPreKey<Curve> &SPk, uint32_t &OPk_id) {
				X<Curve, lime::Xtype::publicKey> Ek{};
				uint32_t SPk_id=0;
				bool OPk_flag=false;
				double_ratchet_protocol::parseMessage_X3DHinit(X3DH_initMessage, peerIk, Ek, SPk_id, OPk_id, OPk_flag);
				SPk = get_SPk(SPk_id); // this one will throw an exception if the SPk is not found in local storage, let it flow up

				// Compute 	DH1 = DH(SPk, peer Ik)
				// 		DH2 = DH(self Ik, Ek)
				// 		DH3 = DH(SPk, Ek)
				// 		DH4 = DH(OPk, Ek)  if peer used an OPk

				// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F: keyLength bytes set to 0xFF
				// use sBuffer of size able to hold also DH$ even if we may not use it
				sBuffer<DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4> HKDF_input;
				HKDF_input.fill(0xFF); // HKDF_input holds F
				size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

				auto DH = make_keyExchange<Curve>();

				// DH1 (SPk, peerIk)
				DH->set_secret(SPk.cprivateKey());
				DH->set_selfPublic(SPk.cpublicKey());
				DH->set_peerPublic(peerIk); // peer Ik key is converted from Signature to key exchange format
				DH->computeSharedSecret();
				auto DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
				HKDF_input_index += DH_out.size();

				// Then DH3 = DH(SPk, Ek) as we already have SPk in the key Exchange context, we will go back for DH2 after this one
				DH->set_peerPublic(Ek);
				DH->computeSharedSecret();
				DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

				// DH2 = DH(self Ik, Ek), Ek is already DH context
				// convert self ED Ik pair into X keys
				load_SelfIdentityKey(); // make sure self IK is in context
				DH->set_secret(m_Ik.privateKey()); // self Ik key is converted from Signature to key exchange format
				DH->set_selfPublic(m_Ik.publicKey());
				DH->computeSharedSecret();
				DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
				HKDF_input_index += 2*DH_out.size();

				if (OPk_flag) { // there is an OPk id
					const auto OPk = get_OPk(OPk_id); // this one will throw an exception if the OPk is not found in local storage, let it flow up

					// DH4 = DH(OPk, Ek) Ek is already in context
					DH->set_secret(OPk.cprivateKey());
					DH->set_selfPublic(OPk.cpublicKey());
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
					HKDF_input_index += DH_out.size();
				}

				DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

				// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4) (DH4 optionnal)
				DRChainKey SK;
				/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
				std::vector<uint8_t> salt(SHA512::ssize(), 0);
				HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, lime::settings::X3DH_SK_info.data(), lime::settings::X3DH_SK_info.size(), SK.data(), SK.size());

				return SK;
			}
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			DRChainKey X3DH_receiver(const std::vector<uint8_t> X3DH_initMessage, DSA<typename Curve::EC, lime::DSAtype::publicKey> &peerIk, SignedPreKey<Curve> &SPk, uint32_t &OPk_id) {
				X<typename Curve::EC, lime::Xtype::publicKey> Ek{};
				K<typename Curve::KEM, lime::Ktype::cipherText> Ct{};
				uint32_t SPk_id=0;
				bool OPk_flag=false;
				double_ratchet_protocol::parseMessage_X3DHinit<Curve>(X3DH_initMessage, peerIk, Ek, Ct, SPk_id, OPk_id, OPk_flag);
				SPk = get_SPk(SPk_id); // this one will throw an exception if the SPk is not found in local storage, let it flow up
				// Compute 	DH1 = DH(SPk, peer Ik)
				// 		DH2 = DH(self Ik, Ek)
				// 		DH3 = DH(SPk, Ek)
				// 		DH4 = DH(OPk, Ek)  if peer used an OPk
				// 		KEM1 = encaps(OPk)  if peer used an OPk or encaps(SPk)

				// Initiate HKDF input : We will compute HKDF with a concat of F and all DH/KEM computed, see X3DH spec section 2.2 for what is F: keyLength bytes set to 0xFF
				// The derivation also include a transcript of all public key used: IkA || EkA || IkB || SPkB || OPkB(if present) || KEM-Key (OPk or SPk if PQ-OPk is not present) || KEM Cipher text
				// use sBuffer of size able to hold also DH$ even if we may not use it
				sBuffer<
					DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4 + K<Curve, lime::Ktype::sharedSecret>::ssize() // secrets
					+ DSA<Curve, lime::DSAtype::publicKey>::ssize() +  X<Curve, lime::Xtype::sharedSecret>::ssize() // IkA || EkA
					+ DSA<Curve, lime::DSAtype::publicKey>::ssize() +  X<Curve, lime::Xtype::sharedSecret>::ssize()*2 // IkB || SPkB || OPkB
					+ K<Curve, lime::Ktype::publicKey>::ssize() + K<Curve, lime::Ktype::cipherText>::ssize() // KEM-OPk or KEM-SPk || Kem cipher text
				> HKDF_input;
				HKDF_input.fill(0xFF); // HKDF_input holds F
				size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

				auto DH = make_keyExchange<typename Curve::EC>();

				// DH1 (SPk, peerIk)
				DH->set_secret(SPk.cECprivateKey());
				DH->set_selfPublic(SPk.cECpublicKey());
				DH->set_peerPublic(peerIk); // peer Ik key is converted from Signature to key exchange format
				DH->computeSharedSecret();
				auto DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
				HKDF_input_index += DH_out.size();

				// Then DH3 = DH(SPk, Ek) as we already have SPk in the key Exchange context, we will go back for DH2 after this one
				DH->set_peerPublic(Ek);
				DH->computeSharedSecret();
				DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

				// DH2 = DH(self Ik, Ek), Ek is already DH context
				// convert self ED Ik pair into X keys
				load_SelfIdentityKey(); // make sure self IK is in context
				DH->set_secret(m_Ik.privateKey()); // self Ik key is converted from Signature to key exchange format
				DH->set_selfPublic(m_Ik.publicKey());
				DH->computeSharedSecret();
				DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
				HKDF_input_index += 2*DH_out.size();

				// If an OPk is provided, it means sender encapsulated a secret to this key
				auto KEMengine = make_KEM<typename Curve::KEM>();
				K<typename Curve::KEM, lime::Ktype::sharedSecret> sharedSecret{};
				lime::OneTimePreKey<Curve> OPk{};
				if (OPk_flag) { // there is an OPk id
					OPk = get_OPk(OPk_id); // this one will throw an exception if the OPk is not found in local storage, let it flow up

					// DH4 = DH(OPk, Ek) Ek is already in context
					DH->set_secret(OPk.cECprivateKey());
					DH->set_selfPublic(OPk.cECpublicKey());
					DH->computeSharedSecret();
					DH_out = DH->get_sharedSecret();
					std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
					HKDF_input_index += DH_out.size();
					KEMengine->decaps(OPk.cKEMprivateKey(), Ct, sharedSecret);
				} else { // No OPk provided, sender encapsulated a secret to the SPk
					KEMengine->decaps(SPk.cKEMprivateKey(), Ct, sharedSecret);
				}
				std::copy_n(sharedSecret.cbegin(), sharedSecret.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || [DH4] || KEM1
				HKDF_input_index += sharedSecret.size();

				// Append the transcript to the HKDF input: IkA || EkA || IkB || EC-SPkB || [EC-OPkB || KEM-OPkB] OR [KEM-SPkB] || KEM cipherText
				std::copy_n(peerIk.cbegin(), peerIk.size(), HKDF_input.begin()+HKDF_input_index); // IkA
				HKDF_input_index += peerIk.size();
				std::copy_n(Ek.cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), HKDF_input.begin()+HKDF_input_index); // EkA
				HKDF_input_index += X<Curve, lime::Xtype::publicKey>::ssize();
				std::copy_n(m_Ik.publicKey().cbegin(), m_Ik.publicKey().size(), HKDF_input.begin()+HKDF_input_index); // IkB
				HKDF_input_index += m_Ik.publicKey().size();
				std::copy_n(SPk.cECpublicKey().cbegin(), SPk.cECpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // EC-SPkB
				HKDF_input_index += SPk.cECpublicKey().size();
				if (OPk_flag) {
					std::copy_n(OPk.cECpublicKey().cbegin(), OPk.cECpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // EC-OPkB
					HKDF_input_index += OPk.cECpublicKey().size();
					std::copy_n(OPk.cKEMpublicKey().cbegin(), OPk.cKEMpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // KEM-OPkB
					HKDF_input_index += OPk.cKEMpublicKey().size();
				} else {
					std::copy_n(SPk.cKEMpublicKey().cbegin(), SPk.cKEMpublicKey().size(), HKDF_input.begin()+HKDF_input_index); // KEM-SPkB
					HKDF_input_index += SPk.cKEMpublicKey().size();
				}
				std::copy_n(Ct.cbegin(), Ct.size(), HKDF_input.begin()+HKDF_input_index); // KEM-cipherText
				HKDF_input_index += Ct.size();

				DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore
				KEMengine = nullptr;

				// Compute SK = HKDF(F || DH1 || DH2 || DH3 || [DH4] || KEM1
				DRChainKey SK;
				/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
				/* the info label as specified in PQXDH section 2.2 : <Identifier>_<EC algo Id>_<hash algo id>_<kem algo id> */
				std::vector<uint8_t> salt(SHA512::ssize(), 0);
				std::string HKDF_info(lime::settings::X3DH_SK_info);
				HKDF_info.append("_").append(Curve::EC::Id()).append("_SHA512_").append(Curve::KEM::Id());
				HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, HKDF_info.data(), HKDF_info.size(), SK.data(), SK.size());

				return SK;
			}

	public:
			/********************************************************************************/
			/*                               Constructor                                    */
			/********************************************************************************/
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			X3DHi(std::shared_ptr< lime::Db > localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL,  const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr< lime::RNG > RNG_context, const long int Uid) :
			m_RNG{RNG_context}, m_selfDeviceId{selfDeviceId}, m_localStorage{localStorage}, m_db_Uid{Uid},
			m_server_url{X3DHServerURL}, m_post_data{X3DH_post_data},
			m_Ik_loaded{false} {
				if (Uid == 0) { // When the given user id is 0: we must create the user
					std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
					int dbUid;
					int curve;

					// check if the user is not already in the DB
					m_localStorage->sql<<"SELECT Uid,curveId FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(dbUid), into(curve), use(selfDeviceId);
					if (m_localStorage->sql.got_data()) {
						if (curve&lime::settings::DBInactiveUserBit) { // user is there but inactive, just return true, the insert_LimeUser will try to publish it again
							m_db_Uid = dbUid;
							return;
						} else {
							throw BCTBX_EXCEPTION << "Lime user "<<selfDeviceId<<" cannot be created: it is already in Database - delete it before if you really want to replace it";
						}
					}

					// generate an identity Signature key pair
					auto IkSig = make_Signature<Curve>();
					IkSig->createKeyPair(m_RNG);

					// store it in a blob : Public||Private
					blob Ik(m_localStorage->sql);
					Ik.write(0, (const char *)(IkSig->get_public().data()), DSA<Curve, lime::DSAtype::publicKey>::ssize());
					Ik.write(DSA<Curve, lime::DSAtype::publicKey>::ssize(), (const char *)(IkSig->get_secret().data()), DSA<Curve, lime::DSAtype::privateKey>::ssize());

					// set the Ik in Lime object?
					//m_Ik = std::move(KeyPair<ED<Curve>>{EDDSAContext->publicKey, EDDSAContext->secretKey});

					transaction tr(m_localStorage->sql);

					// insert in DB
					try {
						// Don't create stack variable in the method call directly
						// set the inactive user bit on, user is not active until X3DH server's confirmation
						int curveId = lime::settings::DBInactiveUserBit | static_cast<uint16_t>(Curve::curveId());

						m_localStorage->sql<<"INSERT INTO lime_LocalUsers(UserId,Ik,server,curveId,updateTs) VALUES (:userId,:Ik,:server,:curveId, CURRENT_TIMESTAMP) ", use(selfDeviceId), use(Ik), use(X3DHServerURL), use(curveId);
					} catch (exception const &e) {
						tr.rollback();
						throw BCTBX_EXCEPTION << "Lime user insertion failed. DB backend says: "<<e.what();
					}
					// get the Id of inserted row
					m_localStorage->sql<<"select last_insert_rowid()",into(m_db_Uid);

					tr.commit();
					/* WARNING: previous line break portability of DB backend, specific to sqlite3.
					Following code shall work but consistently returns false and do not set m_db_Uid...*/
					/*
					if (!(m_localStorage->sql.get_last_insert_id("lime_LocalUsers", m_db_Uid)))
						throw BCTBX_EXCEPTION << "Lime user insertion failed. Couldn't retrieve last insert DB";
					}
					*/
					/* all went fine set the Ik loaded flag */
					//m_Ik_loaded = true;
				}
			}

			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			X3DHi(std::shared_ptr< lime::Db > localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL,  const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr< lime::RNG > RNG_context, const long int Uid) :
			m_RNG{RNG_context}, m_selfDeviceId{selfDeviceId}, m_localStorage{localStorage}, m_db_Uid{Uid},
			m_server_url{X3DHServerURL}, m_post_data{X3DH_post_data},
			m_Ik_loaded{false} {
				if (Uid == 0) { // When the given user id is 0: we must create the user
					std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
					int dbUid;
					int curve;

					// check if the user is not already in the DB
					m_localStorage->sql<<"SELECT Uid,curveId FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(dbUid), into(curve), use(selfDeviceId);
					if (m_localStorage->sql.got_data()) {
						if (curve&lime::settings::DBInactiveUserBit) { // user is there but inactive, just return true, the insert_LimeUser will try to publish it again
							m_db_Uid = dbUid;
							return;
						} else {
							throw BCTBX_EXCEPTION << "Lime user "<<selfDeviceId<<" cannot be created: it is already in Database - delete it before if you really want to replace it";
						}
					}

					// generate an identity Signature key pair
					auto IkSig = make_Signature<typename Curve::EC>();
					IkSig->createKeyPair(m_RNG);

					// store it in a blob : Public||Private
					blob Ik(m_localStorage->sql);
					Ik.write(0, (const char *)(IkSig->get_public().data()), DSA<Curve, lime::DSAtype::publicKey>::ssize());
					Ik.write(DSA<Curve, lime::DSAtype::publicKey>::ssize(), (const char *)(IkSig->get_secret().data()), DSA<Curve, lime::DSAtype::privateKey>::ssize());

					// set the Ik in Lime object?
					//m_Ik = std::move(KeyPair<ED<Curve>>{EDDSAContext->publicKey, EDDSAContext->secretKey});

					transaction tr(m_localStorage->sql);

					// insert in DB
					try {
						// Don't create stack variable in the method call directly
						// set the inactive user bit on, user is not active until X3DH server's confirmation
						int curveId = lime::settings::DBInactiveUserBit | static_cast<uint16_t>(Curve::curveId());

						m_localStorage->sql<<"INSERT INTO lime_LocalUsers(UserId,Ik,server,curveId,updateTs) VALUES (:userId,:Ik,:server,:curveId, CURRENT_TIMESTAMP) ", use(selfDeviceId), use(Ik), use(X3DHServerURL), use(curveId);
					} catch (exception const &e) {
						tr.rollback();
						throw BCTBX_EXCEPTION << "Lime user insertion failed. DB backend says: "<<e.what();
					}
					// get the Id of inserted row
					m_localStorage->sql<<"select last_insert_rowid()",into(m_db_Uid);

					tr.commit();
					/* WARNING: previous line break portability of DB backend, specific to sqlite3.
					Following code shall work but consistently returns false and do not set m_db_Uid...*/
					/*
					if (!(m_localStorage->sql.get_last_insert_id("lime_LocalUsers", m_db_Uid)))
						throw BCTBX_EXCEPTION << "Lime user insertion failed. Couldn't retrieve last insert DB";
					}
					*/
					/* all went fine set the Ik loaded flag */
					//m_Ik_loaded = true;
				}
			}

			X3DHi() = delete; // make sure the X3DH is not initialised without parameters
			X3DHi(X3DHi<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			X3DHi<Curve> &operator=(X3DHi<Curve> &a) = delete; // can't copy a session
			~X3DHi() {};

			/********************************************************************************/
			/*                        X3DH interface implementation                         */
			/********************************************************************************/
			std::string get_x3dhServerUrl(void) override {
				return m_server_url;
			}

			void set_x3dhServerUrl(const std::string &x3dhServerUrl) override {
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
				transaction tr(m_localStorage->sql);

				// update in DB, do not check presence as we're called after a load_user who already ensure that
				try {
					m_localStorage->sql<<"UPDATE lime_LocalUsers SET server = :server WHERE UserId = :userId;", use(x3dhServerUrl), use(m_selfDeviceId);
				} catch (exception const &e) {
					tr.rollback();
					throw BCTBX_EXCEPTION << "Cannot set the X3DH server url for user "<<m_selfDeviceId<<". DB backend says: "<<e.what();
				}
				// update in the X3DH object
				m_server_url = x3dhServerUrl;

				tr.commit();
			}

			void get_Ik(std::vector<uint8_t> &Ik) override {
				load_SelfIdentityKey(); // make sure we have the key
				Ik.assign(m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend());
			}

			long int get_dbUid(void) const noexcept override {return m_db_Uid;} // the Uid in database, retrieved at creation/load, used for faster access
			void publish_user(std::shared_ptr<callbackUserData> userData, uint16_t OPkInitialBatchSize) override{
				// Generate (or load if they already are in base when publishing an inactive user) the SPk
				auto SPk = generate_SPk(true);

				// Generate (or load if they already are in base when publishing an inactive user) the OPks
				std::vector<OneTimePreKey<Curve>> OPks{};
				generate_OPks(OPks, OPkInitialBatchSize, true);

				// Build and post the message to server
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_registerUser<Curve>(X3DHmessage, m_Ik.publicKey(), SPk, OPks);
				postToX3DHServer(userData, X3DHmessage);
			}

			void delete_user(std::shared_ptr<callbackUserData> userData) override {
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_deleteUser<Curve>(X3DHmessage);
				postToX3DHServer(userData, X3DHmessage);
			}

			bool is_currentSPk_valid(void) override{
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
				// Do we have an active SPk for this user which is younger than SPK_lifeTime_days
				int dummy;
				m_localStorage->sql<<"SELECT SPKid FROM X3DH_SPk WHERE Uid = :Uid AND Status = 1 AND timeStamp > date('now', '-"<<lime::settings::SPK_lifeTime_days<<" day') LIMIT 1;", into(dummy), use(m_db_Uid);
				if (m_localStorage->sql.got_data()) {
					return true;
				} else {
					return false;
				}
			}

			void update_SPk(std::shared_ptr<callbackUserData> userData) override {
				// generate and publish the SPk
				auto SPk = generate_SPk();
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_publishSPk(X3DHmessage, SPk);
				postToX3DHServer(userData, X3DHmessage);
			}

			void update_OPk(std::shared_ptr<callbackUserData> userData) override {
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_getSelfOPks<Curve>(X3DHmessage);
				postToX3DHServer(userData, X3DHmessage); // in the response from server, if more OPks are needed, it will generate and post them before calling the callback
			}

			void fetch_peerBundles(std::shared_ptr<callbackUserData> userData, std::vector<std::string> &peerDeviceIds) override {
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_getPeerBundles<Curve>(X3DHmessage, peerDeviceIds);
				postToX3DHServer(userData, X3DHmessage);
			}

			std::shared_ptr<DR> init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId) override {
				DSA<typename Curve::EC, lime::DSAtype::publicKey> peerIk{};
				SignedPreKey<Curve> SPk{};
				uint32_t OPk_id=0;

				auto SK = X3DH_receiver(X3DH_initMessage, peerIk, SPk, OPk_id);

				// Generate the shared AD used in DR session
				SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id), we are receiver on this one
				std::vector<uint8_t> AD_input{peerIk.cbegin(), peerIk.cend()};
				AD_input.insert(AD_input.end(), m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend());
				AD_input.insert(AD_input.end(), senderDeviceId.cbegin(), senderDeviceId.cend());
				AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
				/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
				std::vector<uint8_t> salt(SHA512::ssize(), 0);
				HMAC_KDF<SHA512>(salt.data(), salt.size(), AD_input.data(), AD_input.size(), lime::settings::X3DH_AD_info.data(), lime::settings::X3DH_AD_info.size(), AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

				// check the new peer device Id in Storage, if it is not found, the DR session will add it when it saves itself after successful decryption
				auto peerDid = m_localStorage->check_peerDevice(senderDeviceId, peerIk);
				auto DRSession = make_DR_for_receiver<Curve>(m_localStorage, SK, AD, SPk, peerDid, senderDeviceId, OPk_id, peerIk, m_db_Uid, m_RNG);

				return std::static_pointer_cast<DR>(DRSession);
			}
	};

	/****************************************************************************/
	/* factory functions                                                        */
	/****************************************************************************/
	template <typename Algo> std::shared_ptr<X3DH> make_X3DH(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid) {
		return std::static_pointer_cast<X3DH>(std::make_shared<X3DHi<Algo>>(localStorage, selfDeviceId, X3DHServerURL, X3DH_post_data, RNG_context, Uid));
	}

/* template instanciations */
#ifdef EC25519_ENABLED
	template std::shared_ptr<X3DH> make_X3DH<C255>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId,const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
#ifdef EC448_ENABLED
	template std::shared_ptr<X3DH> make_X3DH<C448>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId,const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
#ifdef HAVE_BCTBXPQ
	template std::shared_ptr<X3DH> make_X3DH<C255K512>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
} // namespace lime
