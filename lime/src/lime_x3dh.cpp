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
			/* local storage related */
			std::shared_ptr<lime::Db> m_localStorage; // shared pointer would be used/stored in Double Ratchet Sessions
			long int m_db_Uid; // the Uid in database, retrieved at creation/load, used for faster access

			/* X3DH keys */
			DSApair<Curve> m_Ik; // our identity key pair, is loaded from DB only if requested(to sign a SPK or to perform X3DH init)
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
			SignedPreKey<Curve> X3DH_generate_SPk(const DSApair<Curve> &Ik, const bool load=false) {
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
						SPkSign->set_public(Ik.cpublicKey());
						SPkSign->set_secret(Ik.cprivateKey());
						SPkSign->sign(s.publicKey(), s.signature());
						return s;
					}
				}
					// Generate a new ECDH Key pair
				auto DH = make_keyExchange<Curve>();
				DH->createKeyPair(m_RNG);
				SignedPreKey<Curve> s(DH->get_selfPublic(), DH->get_secret());

				// Sign the public key with our identity key
				auto SPkSign = make_Signature<Curve>();
				SPkSign->set_public(Ik.cpublicKey());
				SPkSign->set_secret(Ik.cprivateKey());
				SPkSign->sign(s.cpublicKey(), s.signature());

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

			void X3DH_generate_OPks(std::vector<OneTimePreKey<Curve>> &OPks, const uint16_t OPk_number, const bool load) {

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
	public:
			/** Constructors **/
			/** User already in local storage: just register the Uid and needed info, self Identity key will be loaded only when needed */
			X3DHi<Curve>(std::shared_ptr< lime::Db > localStorage, const long int UId, std::shared_ptr< lime::RNG > RNG_context) : m_RNG{RNG_context}, m_localStorage{localStorage}, m_db_Uid{UId}, m_Ik_loaded{false}{};
			/** User must be created in local storage: create the Ik only, the rest of it will be done when publishing it **/
			X3DHi<Curve>(std::shared_ptr< lime::Db > localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, std::shared_ptr< lime::RNG > RNG_context) : m_RNG{RNG_context}, m_localStorage{localStorage}, m_db_Uid{0}, m_Ik_loaded{false}{
				std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
				int Uid;
				int curve;

				// check if the user is not already in the DB
				m_localStorage->sql<<"SELECT Uid,curveId FROM lime_LocalUsers WHERE UserId = :userId LIMIT 1;", into(Uid), into(curve), use(selfDeviceId);
				if (m_localStorage->sql.got_data()) {
					if (curve&lime::settings::DBInactiveUserBit) { // user is there but inactive, just return true, the insert_LimeUser will try to publish it again
						m_db_Uid = Uid;
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
			};

			X3DHi() = delete; // make sure the X3DH is not initialised without parameters
			X3DHi(X3DHi<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			X3DHi<Curve> &operator=(X3DHi<Curve> &a) = delete; // can't copy a session
			~X3DHi() {};

			/** X3DH interface implementation **/
			long int get_dbUid(void) const noexcept override {return m_db_Uid;} // the Uid in database, retrieved at creation/load, used for faster access
			std::vector<uint8_t> publish_user(const uint16_t OPkInitialBatchSize) override{
				load_SelfIdentityKey(); // make sure our Ik is loaded in object
				// Generate (or load if they already are in base when publishing an inactive user) the SPk
				auto SPk = X3DH_generate_SPk(m_Ik, true);

				// Generate (or load if they already are in base when publishing an inactive user) the OPks
				std::vector<OneTimePreKey<Curve>> OPks{};
				X3DH_generate_OPks(OPks, OPkInitialBatchSize, true);

				// Build and post the message to server
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_registerUser<Curve>(X3DHmessage, m_Ik.publicKey(), SPk, OPks);
				return X3DHmessage;
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
			std::vector<uint8_t> update_SPk(void) override {
				load_SelfIdentityKey(); // make sure our Ik is loaded in object
				// generate and publish the SPk
				auto SPk = X3DH_generate_SPk(m_Ik);
				std::vector<uint8_t> X3DHmessage{};
				x3dh_protocol::buildMessage_publishSPk(X3DHmessage, SPk);
				return X3DHmessage;
			}

			std::vector<uint8_t> get_Ik(void) override {return {};}
	};

	/****************************************************************************/
	/* factory functions                                                        */
	/****************************************************************************/
	template <typename Algo> std::shared_ptr<X3DH> make_X3DH(std::shared_ptr<lime::Db> localStorage, const long UId, std::shared_ptr<RNG> RNG_context) {
		return std::static_pointer_cast<X3DH>(std::make_shared<X3DHi<Algo>>(localStorage, UId, RNG_context));
	}
	template <typename Algo> std::shared_ptr<X3DH> make_X3DH(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId,  const std::string &X3DHServerURL, std::shared_ptr<RNG> RNG_context) {
		return std::static_pointer_cast<X3DH>(std::make_shared<X3DHi<Algo>>(localStorage, selfDeviceId, X3DHServerURL, RNG_context));
	}

/* template instanciations */
#ifdef EC25519_ENABLED
	template std::shared_ptr<X3DH> make_X3DH<C255>(std::shared_ptr<lime::Db> localStorage, const long UId, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<X3DH> make_X3DH<C255>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, std::shared_ptr<RNG> RNG_context);
#endif
#ifdef EC448_ENABLED
	template std::shared_ptr<X3DH> make_X3DH<C448>(std::shared_ptr<lime::Db> localStorage, const long UId, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<X3DH> make_X3DH<C448>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, std::shared_ptr<RNG> RNG_context);
#endif

	/**
	 * @brief Get a vector of peer bundle and initiate a DR Session with it. Created sessions are stored in lime cache and db along the X3DH init packet
	 *  as decribed in X3DH reference section 3.3
	 */
	template <typename Curve>
	void Lime<Curve>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<Curve>> &peersBundle) {
		for (const auto &peerBundle : peersBundle) {
			// do we have a key bundle to build this message from ?
			if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
				continue;
			}
			// Verifify SPk_signature, throw an exception if it fails
			auto SPkVerify = make_Signature<Curve>();
			SPkVerify->set_public(peerBundle.Ik);

			if (!SPkVerify->verify(peerBundle.SPk.cpublicKey(), peerBundle.SPk.csignature())) {
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
			get_SelfIdentityKey(); // make sure it is in context
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
			if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
				m_DR_sessions_cache.erase(peerBundle.deviceId); // will just do nothing if this peerDeviceId is not in cache
			}

			m_DR_sessions_cache.emplace(peerBundle.deviceId, std::static_pointer_cast<DR>(make_DR_for_sender<Curve>(m_localStorage, SK, AD, peerBundle.SPk, peerDid, peerBundle.deviceId, peerBundle.Ik, m_db_Uid, X3DH_initMessage, m_RNG))); // will just do nothing if this peerDeviceId is already in cache

			LIME_LOGI<<"X3DH created session with device "<<peerBundle.deviceId;
		}
	}

	template <typename Curve>
	std::shared_ptr<DR> Lime<Curve>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId) {
		DSA<Curve, lime::DSAtype::publicKey> peerIk{};
		X<Curve, lime::Xtype::publicKey> Ek{};
		bool OPk_flag = false;
		uint32_t SPk_id=0, OPk_id=0;

		double_ratchet_protocol::parseMessage_X3DHinit(X3DH_initMessage, peerIk, Ek, SPk_id, OPk_id, OPk_flag);

		auto SPk = X3DH_get_SPk(SPk_id); // this one will throw an exception if the SPk is not found in local storage, let it flow up

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
		DH->set_secret(SPk.privateKey());
		DH->set_selfPublic(SPk.publicKey());
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
		get_SelfIdentityKey(); // make sure self IK is in context
		DH->set_secret(m_Ik.privateKey()); // self Ik key is converted from Signature to key exchange format
		DH->set_selfPublic(m_Ik.publicKey());
		DH->computeSharedSecret();
		DH_out = DH->get_sharedSecret();
		std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
		HKDF_input_index += 2*DH_out.size();

		if (OPk_flag) { // there is an OPk id
			const auto OPk = X3DH_get_OPk(OPk_id); // this one will throw an exception if the OPk is not found in local storage, let it flow up

			// DH4 = DH(OPk, Ek) Ek is already in context
			DH->set_secret(OPk.cprivateKey());
			DH->set_selfPublic(OPk.cpublicKey());
			DH->computeSharedSecret();
			DH_out = DH->get_sharedSecret();
			std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 DH4
			HKDF_input_index += DH_out.size();
		}

		DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

		// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4) (DH4 optionnal)
		DRChainKey SK;
		/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
		std::vector<uint8_t> salt(SHA512::ssize(), 0);
		HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, lime::settings::X3DH_SK_info.data(), lime::settings::X3DH_SK_info.size(), SK.data(), SK.size());

		// Generate the shared AD used in DR session
		SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id), we are receiver on this one
		std::vector<uint8_t> AD_input{peerIk.cbegin(), peerIk.cend()};
		AD_input.insert(AD_input.end(), m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend());
		AD_input.insert(AD_input.end(), senderDeviceId.cbegin(), senderDeviceId.cend());
		AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
		HMAC_KDF<SHA512>(salt.data(), salt.size(), AD_input.data(), AD_input.size(), lime::settings::X3DH_AD_info.data(), lime::settings::X3DH_AD_info.size(), AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

		// check the new peer device Id in Storage, if it is not found, the DR session will add it when it saves itself after successful decryption
		auto peerDid = m_localStorage->check_peerDevice(senderDeviceId, peerIk);
		auto DRSession = make_DR_for_receiver<Curve>(m_localStorage, SK, AD, SPk, peerDid, senderDeviceId, OPk_flag?OPk_id:0, peerIk, m_db_Uid, m_RNG);

		return std::static_pointer_cast<DR>(DRSession);
	}

	/* Instanciate templated member functions */
#ifdef EC25519_ENABLED
	template void Lime<C255>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<C255>> &peerBundle);
	template std::shared_ptr<DR> Lime<C255>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &peerDeviceId);
#endif

#ifdef EC448_ENABLED
	template void Lime<C448>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<C448>> &peerBundle);
	template std::shared_ptr<DR> Lime<C448>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &peerDeviceId);
#endif

}
