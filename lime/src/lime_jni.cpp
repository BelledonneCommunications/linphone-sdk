/**
	@file lime_jni.cpp

	@author Johan Pascal

	@copyright	Copyright (C) 2019  Belledonne Communications SARL

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
#include <jni/jni.hpp>

#include "lime_log.hpp"
#include <lime/lime.hpp>
#include "bctoolbox/exception.hh"

/**
 * @brief Holds a stateful function pointer to be called to process the X3DH server response
 * Encapsulate the function pointer in an object to pass its pointer to the java side
 * as it cannot manage stateful function pointers
 */
struct responseHolder {
	const lime::limeX3DHServerResponseProcess process;

	responseHolder(const lime::limeX3DHServerResponseProcess &process) : process{process} {};
};


static void jbyteArray2uin8_tVector(jni::JNIEnv &env, const jni::Array<jni::jbyte> &in, std::shared_ptr<std::vector<uint8_t>> &out) {
	// turn the in array into a vector of jbytes(signed char)
	std::vector<int8_t> signedResponseVector = jni::Make<std::vector<jbyte>>(env, in);

	// copy it to an unsigned char(not the nost efficient way but c++ seems to be unable to directly cast a vector in a clean way)
	for (auto &sb : signedResponseVector) {
		out->push_back(reinterpret_cast<uint8_t &>(sb));
	}
}

/**
 * @brief convert an int mapped java enumerated peerDevice Status into a c++ one
 *
 * mapping is :
 * 	UNTRUSTED(0) TRUSTED(1) UNSAFE(2) FAIL(3) UNKNOWN(4)
 *
 * @param[in]	peerStatus	The java mapped integer to a curveId enum
 * @return the c++ enumerated peer device status (silently default to unknown)
 */
static lime::PeerDeviceStatus j2cPeerDeviceStatus(jni::jint peerStatus) {
	switch (peerStatus) {
		case 0:
			return lime::PeerDeviceStatus::untrusted;
		case 1:
			return lime::PeerDeviceStatus::trusted;
		case 2:
			return lime::PeerDeviceStatus::unsafe;
		case 3:
			return lime::PeerDeviceStatus::fail;
		case 4:
		default:
			return lime::PeerDeviceStatus::unknown;
	}
}

/**
 * @brief convert a c++ enumerated peerDevice Status into a integer one
 *
 * mapping in java is :
 * 	UNTRUSTED(0) TRUSTED(1) UNSAFE(2) FAIL(3) UNKNOWN(4)
 *
 * @param[in]	peerStatus	the c++ enumerated peer Device Status
 * @return the java enumerated peer device status (silently default to 4:UNKNOWN)
 */
static jni::jint c2jPeerDeviceStatus(lime::PeerDeviceStatus peerStatus) {
	switch (peerStatus) {
		case lime::PeerDeviceStatus::untrusted:
			return 0;
		case lime::PeerDeviceStatus::trusted:
			return 1;
		case lime::PeerDeviceStatus::unsafe:
			return 2;
		case lime::PeerDeviceStatus::fail:
			return 3;
		case lime::PeerDeviceStatus::unknown:
		default:
			return 4;
	}
}

/**
 * @brief convert a c++ enumerated CallbackReturn into a integer one
 *
 * mapping in java is :
 * 	SUCCESS(0) FAIL(1)
 *
 * @param[in]	status	the c++ enumerated peer Device Status
 * @return the java enumerated peer device status (silently default to 1:FAIL)
 */
static jni::jint c2jCallbackReturn(lime::CallbackReturn status) {
	switch (status) {
		case lime::CallbackReturn::success:
			return 0;
		case lime::CallbackReturn::fail:
		default:
			return 1;
	}
}

/**
 * @brief convert a int mapped java enumerated encryptionPolicy into a c++ one
 *
 * mapping is :
 * 	DRMESSAGE(0) CIPHERMESSAGE(1) OPTIMIZEUPLOADSIZE(2) OPTIMIZEGLOBALBANDWIDTH(3)
 *
 * @param[in]	encryptionPolicy	The java mapped integer to an encryption policy enum
 * @return the c++ enumerated encryption policy (silently default to optimizeUploadSize)
 */
static lime::EncryptionPolicy j2cEncryptionPolicy(jni::jint encryptionPolicy) {
	switch (encryptionPolicy) {
		case 0:
			return lime::EncryptionPolicy::DRMessage;
		case 1:
			return lime::EncryptionPolicy::cipherMessage;
		case 3:
			return lime::EncryptionPolicy::optimizeGlobalBandwidth;
		case 2:
		default:
			return lime::EncryptionPolicy::optimizeUploadSize;
	}
}

/**
 * @brief convert a int mapped java enumerated curveId into a c++ one
 *
 * mapping is :
 * 	C25519(1) C448(2)
 *
 * @param[in]	curveId	The java mapped integer to a curveId enum
 * @return the c++ enumerated curveId (silently default to unset)
 */
static lime::CurveId j2cCurveId(jni::jint curveId) {
	switch (curveId) {
		case 1:
			return lime::CurveId::c25519;
		case 2:
			return lime::CurveId::c448;
		default:
			return lime::CurveId::unset;
	}
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {

// java classes we would need to access
struct jPostToX3DH { static constexpr auto Name() { return "org/linphone/lime/LimePostToX3DH"; } };
struct jStatusCallback { static constexpr auto Name() { return "org/linphone/lime/LimeStatusCallback"; } };
struct jRecipientData { static constexpr auto Name() { return "org/linphone/lime/RecipientData"; } };
struct jLimeOutputBuffer { static constexpr auto Name() { return "org/linphone/lime/LimeOutputBuffer"; } };
struct jLimeException { static constexpr auto Name() { return "org/linphone/lime/LimeException"; } };

jni::JNIEnv& env { jni::GetEnv(*vm) };


struct jLimeManager {
	static constexpr auto Name() { return "org/linphone/lime/LimeManager"; } // bind this class to the java LimeManager Class

	std::unique_ptr<lime::LimeManager> m_manager; /**< a unique pointer to the actual lime manager */
	jni::Global<jni::Object<jPostToX3DH>, jni::EnvGettingDeleter> jGlobalPostObj; /**< a global reference to the java postToX3DH object. TODO: unclear if EnvIgnoringDeleter is not a better fit here. */

	inline void ThrowJavaLimeException(JNIEnv& env, const std::string &message) {
		auto LimeExceptionClass = env.FindClass("org/linphone/lime/LimeException");
		env.ThrowNew(LimeExceptionClass, message.data());
	}

	/** @brief Constructor
	 * unlike the native lime manager constructor, this one get only one argument has cpp closure cannot be passed easily to java
	 * @param[in]	db_access	the database access path
	 */
	jLimeManager(JNIEnv &env, const jni::String &db_access, jni::Object<jPostToX3DH> &jpostObj) : jGlobalPostObj{jni::NewGlobal<jni::EnvGettingDeleter>(env, jpostObj)} {
		// turn the argument into a cpp string
		std::string cpp_db_access = jni::Make<std::string>(env, db_access);

		// retrieve the JavaVM
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		// closure must capture pointer to current object to be able to access the jGlobalPostObj field
		auto thiz = this;

		m_manager = std::make_unique<lime::LimeManager>(cpp_db_access, [c_vm, thiz](const std::string &url, const std::string &from, const std::vector<uint8_t> &message, const lime::limeX3DHServerResponseProcess &responseProcess){
			//  (TODO? make sure the current process is attached?)
			jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};
			// Create a Cpp object to hold the reponseProcess closure (cannot give a stateful function to the java side)
			auto process = new responseHolder(responseProcess);
			// retrieve the postToX3DHServer  method
			auto PostClass = jni::Class<jPostToX3DH>::Find(g_env);
			auto PostMethod = PostClass.GetMethod<void (jni::jlong, jni::String, jni::String, jni::Array<jni::jbyte>)>(g_env, "postToX3DHServer");

			// Call the postToX3DHServer method passing it our pointer holding the response process object
			thiz->jGlobalPostObj.Call(g_env, PostMethod, jni::jlong(process), jni::Make<jni::String>(g_env, url), jni::Make<jni::String>(g_env, from), jni::Make<jni::Array<jni::jbyte>>(g_env, reinterpret_cast<const std::vector<int8_t>&>(message)));
		});
	}

	jLimeManager(const jLimeManager&) = delete; // noncopyable

	~jLimeManager() {
		// ressources allocated are held by unique ptr, they self delete.
		LIME_LOGD<<"JNI destructs native ressources";
	}


	void create_user(jni::JNIEnv &env, const jni::String &localDeviceId, const jni::String &serverUrl, const jni::jint jcurveId, const jni::jint jOPkInitialBatchSize, jni::Object<jStatusCallback> &jstatusObj ) {
		LIME_LOGD<<"JNI create_user user "<<jni::Make<std::string>(env, localDeviceId)<<" url "<<jni::Make<std::string>(env, serverUrl);
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		// Here we create a global java reference on our object so we won't loose it even if this is called after the current java call
		// This global java reference is given in a unique pointer(why??), so just turn it into a shared one so we can copy it into the closure
		// We could move it to the closure, but it is itself copied into the create_user argument so it would fail
		// Another solution would be to modify the create_user to have it moving the closure and not copying it, it will be used just once anyway
		auto jstatusObjRef = std::make_shared<jni::Global<jni::Object<jStatusCallback>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jstatusObj));

		auto callback_lambda = [c_vm, jstatusObjRef](const lime::CallbackReturn status, const std::string message){
					// get the env from VM, and retrieve the callback method on StatusCallback class
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");
					// call the callback on the statusObj we got in param when calling create_user
					jstatusObjRef->Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				};

		try {
			m_manager->create_user( jni::Make<std::string>(env, localDeviceId),
					jni::Make<std::string>(env, serverUrl),
					j2cCurveId(jcurveId), jOPkInitialBatchSize, callback_lambda);
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}

	void delete_user(jni::JNIEnv &env, const jni::String &localDeviceId, jni::Object<jStatusCallback> &jstatusObj ) {
		LIME_LOGD<<"JNI delete_user user "<<jni::Make<std::string>(env, localDeviceId)<<std::endl;
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		// see create_user for details on this
		auto jstatusObjRef = std::make_shared<jni::Global<jni::Object<jStatusCallback>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jstatusObj));

		auto callback_lambda = [c_vm, jstatusObjRef](const lime::CallbackReturn status, const std::string message){
					// get the env from VM, and retrieve the callback method on StatusCallback class
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");
					// call the callback on the statusObj we got in param when calling create_user
					jstatusObjRef->Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				};

		try {
			m_manager->delete_user( jni::Make<std::string>(env, localDeviceId), callback_lambda);
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}

	jni::jboolean is_user(jni::JNIEnv &env, const jni::String &localDeviceId ) {
		try {
			if (m_manager->is_user(jni::Make<std::string>(env, localDeviceId))) {
				return JNI_TRUE;
			} else {
				return JNI_FALSE;
			}
		} catch (std::exception const &e) { // catch anything, BctbxException are managed in is_user, so there is a real problem if we get there.
			ThrowJavaLimeException(env, e.what());
			return JNI_FALSE;
		}
	}

	void encrypt(jni::JNIEnv &env,  const jni::String &jlocalDeviceId,  const jni::String &jrecipientUserId, jni::Array<jni::Object<jRecipientData>> &jrecipients,
			jni::Array<jni::jbyte> &jplainMessage,
			jni::Object<jLimeOutputBuffer> &jcipherMessage,
			jni::Object<jStatusCallback> &jstatusObj,
			jni::jint encryptionPolicy) {

		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		LIME_LOGD<<"JNI Encrypt from "<<(jni::Make<std::string>(env, jlocalDeviceId))<<" to user "<<(jni::Make<std::string>(env, jrecipientUserId))<<" to "<<jrecipients.Length(env)<<" recipients"<<std::endl;

		// turn the plain message byte array into a vector of uint8_t
		auto plainMessage = std::make_shared<std::vector<uint8_t>>();
		jbyteArray2uin8_tVector(env, jplainMessage, plainMessage);

		auto cipherMessage = std::make_shared<std::vector<uint8_t>>();

		// turn the array of jRecipientData into a vector of recipientData
		auto recipients = std::make_shared<std::vector<lime::RecipientData>>();
		auto RecipientDataClass = jni::Class<jRecipientData>::Find(env);
		auto RecipientDataDeviceIdField = RecipientDataClass.GetField<jni::String>(env, "deviceId");
		auto RecipientDataPeerStatusField = RecipientDataClass.GetField<jni::jint>(env, "peerStatus");
		auto jrecipientsSize = jrecipients.Length(env);

		for (size_t i=0; i<jrecipientsSize; i++) {
			auto recipient = jrecipients.Get(env, i);
			recipients->emplace_back(jni::Make<std::string>(env, recipient.Get(env, RecipientDataDeviceIdField)));
			recipients->back().peerStatus = j2cPeerDeviceStatus(recipient.Get(env, RecipientDataPeerStatusField));
		}

		// we must have shared_pointer for recipientUserId
		auto recipientUserId = std::make_shared<std::string>(jni::Make<std::string>(env, jrecipientUserId));

		// see create_user for details on this
		auto jstatusObjRef = std::make_shared<jni::Global<jni::Object<jStatusCallback>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jstatusObj));
		auto jrecipientsRef = std::make_shared<jni::Global<jni::Array<jni::Object<jRecipientData>>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jrecipients));
		auto jcipherMessageRef = std::make_shared<jni::Global<jni::Object<jLimeOutputBuffer>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jcipherMessage));

		m_manager->encrypt(jni::Make<std::string>(env, jlocalDeviceId),
			recipientUserId,
			recipients,
			plainMessage,
			cipherMessage,
			[c_vm, jstatusObjRef, jrecipientsRef, jcipherMessageRef, recipients, cipherMessage] (const lime::CallbackReturn status, const std::string message) {
				// get the env from VM
				jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};

				// access to java RecipientData fields
				auto RecipientDataClass = jni::Class<jRecipientData>::Find(g_env);
				auto RecipientDataPeerStatusField = RecipientDataClass.GetField<jni::jint>(g_env, "peerStatus");
				auto RecipientDataDRmessageField = RecipientDataClass.GetField<jni::Array<jni::jbyte>>(g_env, "DRmessage");

				// retrieve the cpp recipients vector and copy back to the jrecipients the peerStatus and DRmessage(if any)
				for (size_t i=0; i<recipients->size(); i++) {
					auto jrecipient = jrecipientsRef->Get(g_env, i); // recipient is the recipientData javaObject
					jrecipient.Set(g_env, RecipientDataPeerStatusField, c2jPeerDeviceStatus((*recipients)[i].peerStatus));
					auto jDRmessage = jni::Make<jni::Array<jni::jbyte>>(g_env, reinterpret_cast<const std::vector<int8_t>&>((*recipients)[i].DRmessage));
					jrecipient.Set(g_env, RecipientDataDRmessageField, jDRmessage);
				}

				// retrieve the LimeOutputBuffer class
				auto LimeOutputBufferClass = jni::Class<jLimeOutputBuffer>::Find(g_env);
				auto LimeOutputBufferField = LimeOutputBufferClass.GetField<jni::Array<jni::jbyte>>(g_env, "buffer");

				// get the cipherMessage out
				// Can't use directly a byte[] in parameter (as we must create it from c++ code) so use an dedicated class encapsulating a byte[]
				auto jcipherMessageArray = jni::Make<jni::Array<jni::jbyte>>(g_env, reinterpret_cast<const std::vector<int8_t>&>(*cipherMessage));
				jcipherMessageRef->Set(g_env, LimeOutputBufferField, jcipherMessageArray);

				// retrieve the callback method on StatusCallback class
				auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
				auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");

				// call the callback on the statusObj we got in param when calling create_user
				jstatusObjRef->Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
			},
			j2cEncryptionPolicy(encryptionPolicy)
		);
	}

	jni::jint decrypt(jni::JNIEnv &env,  const jni::String &jlocalDeviceId,  const jni::String &jrecipientUserId, const jni::String &jsenderDeviceId,
			jni::Array<jni::jbyte> &jDRmessage,
			jni::Array<jni::jbyte> &jcipherMessage,
			jni::Object<jLimeOutputBuffer> &jplainMessage) {

		LIME_LOGD<<"JNI Decrypt from "<<(jni::Make<std::string>(env, jsenderDeviceId))<<" for user "<<(jni::Make<std::string>(env, jrecipientUserId))<<" (device : "<<(jni::Make<std::string>(env, jlocalDeviceId))<<")"<<std::endl;

		// turn the DR and cipher message java byte array into a vector of uint8_t
		auto DRmessage = std::make_shared<std::vector<uint8_t>>();
		jbyteArray2uin8_tVector(env, jDRmessage, DRmessage);
		auto cipherMessage = std::make_shared<std::vector<uint8_t>>();
		jbyteArray2uin8_tVector(env, jcipherMessage, cipherMessage);

		std::vector<uint8_t> plainMessage{};

		auto status = m_manager->decrypt(jni::Make<std::string>(env, jlocalDeviceId),
					jni::Make<std::string>(env, jrecipientUserId),
					jni::Make<std::string>(env, jsenderDeviceId),
					*DRmessage,
					*cipherMessage,
					plainMessage);

		// retrieve the LimeOutputBuffer class
		auto LimeOutputBufferClass = jni::Class<jLimeOutputBuffer>::Find(env);
		auto LimeOutputBufferField = LimeOutputBufferClass.GetField<jni::Array<jni::jbyte>>(env, "buffer");

		// get the cipherMessage out
		// Can't use directly a byte[] in parameter (as we must create it from c++ code) so use an dedicated class encapsulating a byte[]
		auto jplainMessageArray = jni::Make<jni::Array<jni::jbyte>>(env, reinterpret_cast<const std::vector<int8_t>&>(plainMessage));
		jplainMessage.Set(env, LimeOutputBufferField, jplainMessageArray);

		return c2jPeerDeviceStatus(status);
	}

	void update(jni::JNIEnv &env, const jni::String &jlocalDeviceId, jni::Object<jStatusCallback> &jstatusObj, jni::jint jOPkServerLowLimit, jni::jint jOPkBatchSize) {
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		LIME_LOGD<<"JNI update for "<<(jni::Make<std::string>(env, jlocalDeviceId));

		// see create_user for details on this
		auto jstatusObjRef = std::make_shared<jni::Global<jni::Object<jStatusCallback>, jni::EnvGettingDeleter>>(jni::NewGlobal<jni::EnvGettingDeleter>(env, jstatusObj));

		m_manager->update(jni::Make<std::string>(env, jlocalDeviceId),
				[c_vm, jstatusObjRef] (const lime::CallbackReturn status, const std::string message)
				{
					// get the env from VM
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};

					// retrieve the callback method on StatusCallback class
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");

					// call the callback on the statusObj we got in param when calling create_user
					jstatusObjRef->Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				},
				jOPkServerLowLimit, jOPkBatchSize);
	}

	void get_selfIdentityKey(jni::JNIEnv &env, const jni::String &jlocalDeviceId, jni::Object<jLimeOutputBuffer> &jIk) {
		try {
			// retrieve the LimeOutputBuffer class
			auto LimeOutputBufferClass = jni::Class<jLimeOutputBuffer>::Find(env);
			auto LimeOutputBufferField = LimeOutputBufferClass.GetField<jni::Array<jni::jbyte>>(env, "buffer");

			// get the Ik
			std::vector<uint8_t> Ik{};
			m_manager->get_selfIdentityKey(jni::Make<std::string>(env, jlocalDeviceId), Ik);
			auto jIkArray = jni::Make<jni::Array<jni::jbyte>>(env, reinterpret_cast<const std::vector<int8_t>&>(Ik));
			jIk.Set(env, LimeOutputBufferField, jIkArray);
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}

	void set_peerDeviceStatus_Ik(jni::JNIEnv &env, const jni::String &jpeerDeviceId, const jni::Array<jni::jbyte> &jIk, const jni::jint jstatus) {
		try {
			// turn the Ik byte array into a vector of uint8_t
			auto Ik = std::make_shared<std::vector<uint8_t>>();
			jbyteArray2uin8_tVector(env, jIk, Ik);

			m_manager->set_peerDeviceStatus(jni::Make<std::string>(env, jpeerDeviceId), *Ik, j2cPeerDeviceStatus(jstatus));
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}

	void set_peerDeviceStatus(jni::JNIEnv &env, const jni::String &jpeerDeviceId, const jni::jint jstatus) {
		m_manager->set_peerDeviceStatus(jni::Make<std::string>(env, jpeerDeviceId), j2cPeerDeviceStatus(jstatus));
	}

	jni::jint get_peerDeviceStatus(jni::JNIEnv &env, const jni::String &jpeerDeviceId) {
		return c2jPeerDeviceStatus(m_manager->get_peerDeviceStatus(jni::Make<std::string>(env, jpeerDeviceId)));
	}

	void delete_peerDevice(jni::JNIEnv &env, const jni::String &jpeerDeviceId) {
		m_manager->delete_peerDevice(jni::Make<std::string>(env, jpeerDeviceId));
	}

	void set_x3dhServerUrl(jni::JNIEnv &env, const jni::String &jlocalDeviceId, const jni::String &jx3dhServerUrl) {
		try {
			m_manager->set_x3dhServerUrl(jni::Make<std::string>(env, jlocalDeviceId), jni::Make<std::string>(env, jx3dhServerUrl));
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}

	void stale_sessions(jni::JNIEnv &env, const jni::String &jlocalDeviceId, const jni::String &jpeerDeviceId) {
		try {
			m_manager->stale_sessions(jni::Make<std::string>(env, jlocalDeviceId), jni::Make<std::string>(env, jpeerDeviceId));
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
	}


	jni::Local<jni::String> get_x3dhServerUrl(jni::JNIEnv &env, const jni::String &jlocalDeviceId) {
		std::string url{};
		try {
			url = m_manager->get_x3dhServerUrl(jni::Make<std::string>(env, jlocalDeviceId));
		} catch (BctbxException const &e) {
			ThrowJavaLimeException(env, e.str());
		} catch (std::exception const &e) { // catch anything
			ThrowJavaLimeException(env, e.what());
		}
		return jni::Make<jni::String>(env, url);
	}
};

#define METHOD(MethodPtr, name) jni::MakeNativePeerMethod<decltype(MethodPtr), (MethodPtr)>(name)

jni::RegisterNativePeer<jLimeManager>(env, jni::Class<jLimeManager>::Find(env), "nativePtr",
	jni::MakePeer<jLimeManager, jni::String &, jni::Object<jPostToX3DH> &>,
	"initialize",
	"nativeDestructor",
	METHOD(&jLimeManager::create_user, "n_create_user"),
	METHOD(&jLimeManager::delete_user, "delete_user"),
	METHOD(&jLimeManager::is_user, "is_user"),
	METHOD(&jLimeManager::encrypt, "n_encrypt"),
	METHOD(&jLimeManager::decrypt, "n_decrypt"),
	METHOD(&jLimeManager::update, "n_update"),
	METHOD(&jLimeManager::get_selfIdentityKey, "get_selfIdentityKey"),
	METHOD(&jLimeManager::set_peerDeviceStatus_Ik, "n_set_peerDeviceStatus_Ik"),
	METHOD(&jLimeManager::set_peerDeviceStatus, "n_set_peerDeviceStatus"),
	METHOD(&jLimeManager::get_peerDeviceStatus, "n_get_peerDeviceStatus"),
	METHOD(&jLimeManager::delete_peerDevice, "delete_peerDevice"),
	METHOD(&jLimeManager::set_x3dhServerUrl, "set_x3dhServerUrl"),
	METHOD(&jLimeManager::get_x3dhServerUrl, "get_x3dhServerUrl")
	);

// bind the process_response to the static java LimeManager.process_response method
// This is not done in the previous RegisterNativePeer because the method is static
/**
 * @brief process response from X3DH server
 * This function is bind to a java PostToX3DH object process_response method
 * It :
 * - converts from jbytes(signed char) to unsigned char the response array
 * - retrieves from the given back responseHolder pointer the closure pointer to callback the line lib and call it
 */
auto process_X3DHresponse= [](jni::JNIEnv &env, jni::Class<jLimeManager>&, jni::jlong processPtr, jni::jint responseCode, jni::Array<jni::jbyte> &response) {
	// turn the response array into a vector of jbytes(signed char)
	auto responseVector = std::make_shared<std::vector<uint8_t>>();
	jbyteArray2uin8_tVector(env, response, responseVector);
	// retrieve the statefull closure pointer to response processing provided by the lime lib
	auto responseHolderPtr = reinterpret_cast<responseHolder *>(processPtr);
	responseHolderPtr->process(responseCode, *responseVector);
		delete(responseHolderPtr);
	};

jni::RegisterNatives(env, *jni::Class<jLimeManager>::Find(env), jni::MakeNativeMethod("process_X3DHresponse", process_X3DHresponse));

return jni::Unwrap(jni::jni_version_1_2);
} // JNI_OnLoad
