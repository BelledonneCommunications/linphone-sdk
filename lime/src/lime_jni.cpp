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
 * @param[in]	peerStatus	the c++ enumerated peer Device Status
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
 * @return the c++ enumerated encryption policy (silently default to optimizeGlobalBandwidth)
 */
static lime::EncryptionPolicy j2cEncryptionPolicy(jni::jint encryptionPolicy) {
	switch (encryptionPolicy) {
		case 0:
			return lime::EncryptionPolicy::DRMessage;
		case 1:
			return lime::EncryptionPolicy::cipherMessage;
		case 3:
			return lime::EncryptionPolicy::optimizeUploadSize;
		case 2:
		default:
			return lime::EncryptionPolicy::optimizeGlobalBandwidth;
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
struct jPostToX3DH { static constexpr auto Name() { return "org/linphone/lime/PostToX3DH"; } };
struct jStatusCallback { static constexpr auto Name() { return "org/linphone/lime/LimeStatusCallback"; } };
struct jRecipientData { static constexpr auto Name() { return "org/linphone/lime/RecipientData"; } };
struct jLimeOutputBuffer { static constexpr auto Name() { return "org/linphone/lime/LimeOutputBuffer"; } };

jni::JNIEnv& env { jni::GetEnv(*vm) };

/**
 * @brief process response from X3DH server
 * This function is bind to a java PostToX3DH object process_response method
 * It :
 * - converts from jbytes(signed char) to unsigned char the response array
 * - retrieves from the given back responseHolder pointer the closure pointer to callback the line lib and call it
 */
auto process_response = [](jni::JNIEnv &env, jni::Class<jPostToX3DH>& , jni::jlong processPtr, jni::jint responseCode, jni::Array<jni::jbyte> &response) {
	// turn the response array into a vector of jbytes(signed char)
	auto responseVector = std::make_shared<std::vector<uint8_t>>();
	jbyteArray2uin8_tVector(env, response, responseVector);

	// retrieve the statefull closure pointer to response processing provided by the lime lib
	auto responseHolderPtr = reinterpret_cast<responseHolder *>(processPtr);
	responseHolderPtr->process(responseCode, *responseVector);

	delete(responseHolderPtr);
};

// bind the process_response to the java PostToX3DH.process_response method
jni::RegisterNatives(env, *jni::Class<jPostToX3DH>::Find(env), jni::MakeNativeMethod("process_response", process_response));

struct jLimeManager {
	static constexpr auto Name() { return "org/linphone/lime/LimeManager"; } // bind this class to the java LimeManager Class

	std::unique_ptr<lime::LimeManager> m_manager; // a unique pointer to the actual lime manager

	/** @brief Constructor
	 * unlike the native lime manager constructor, this one get only one argument has cpp closure cannot be passed easily to java
	 * @param[in]	db_access	the database access path
	 */
	jLimeManager(JNIEnv &env, const jni::String &db_access) {
		// turn the argument into a cpp string
		std::string cpp_db_access = jni::Make<std::string>(env, db_access);

		// retrieve the JavaVM
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		m_manager = std::make_unique<lime::LimeManager>(cpp_db_access, [c_vm](const std::string &url, const std::string &from, const std::vector<uint8_t> &message, const lime::limeX3DHServerResponseProcess &responseProcess){
			//  (TODO? make sure the current process is attached?)
			jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};

			// Create a Cpp object to hold the reponseProcess closure (cannot give a stateful function to the java side)
			auto process = new responseHolder(responseProcess);

			// Instantiate a java postToX3DH object
			auto PostClass = jni::Class<jPostToX3DH>::Find(g_env);
			auto PostClassCtr  = PostClass.GetConstructor<jni::jlong>(g_env);
			auto PostObj = PostClass.New(g_env, PostClassCtr, jni::jlong(process)); // give it a pointer(casted to jlong) to the responseHolder object

			// retrieve the postToX3DHServer method
			auto PostMethod = PostClass.GetMethod<void (jni::jlong, jni::String, jni::String, jni::Array<jni::jbyte>)>(g_env, "postToX3DHServer");
			// Call the postToX3DHServer method on our object
			PostObj.Call(g_env, PostMethod, jni::jlong(process), jni::Make<jni::String>(g_env, url), jni::Make<jni::String>(g_env, from), jni::Make<jni::Array<jni::jbyte>>(g_env, reinterpret_cast<const std::vector<int8_t>&>(message)));
		});

	}

	jLimeManager(const jLimeManager&) = delete; // noncopyable

	~jLimeManager() {
		m_manager = nullptr; // destroy lime manager object
	}


	void create_user(jni::JNIEnv &env, const jni::String &localDeviceId, const jni::String &serverUrl, const jni::jint jcurveId, const jni::jint jOPkInitialBatchSize, jni::Object<jStatusCallback> &statusObj ) {
		LIME_LOGD<<"JNI create_user user "<<jni::Make<std::string>(env, localDeviceId)<<" url "<<jni::Make<std::string>(env, serverUrl);
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		m_manager->create_user( jni::Make<std::string>(env, localDeviceId),
				jni::Make<std::string>(env, serverUrl),
				j2cCurveId(jcurveId), jOPkInitialBatchSize, [c_vm, &statusObj](const lime::CallbackReturn status, const std::string message){
					// get the env from VM, and retrieve the callback method on StatusCallback class
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");
					// call the callback on the statusObj we got in param when calling create_user
					statusObj.Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				});
	}

	void delete_user(jni::JNIEnv &env, const jni::String &localDeviceId, jni::Object<jStatusCallback> &statusObj ) {
		LIME_LOGD<<"JNI delete_user user "<<jni::Make<std::string>(env, localDeviceId)<<std::endl;
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);
		m_manager->delete_user( jni::Make<std::string>(env, localDeviceId),
				[c_vm, &statusObj](const lime::CallbackReturn status, const std::string message){
					// get the env from VM, and retrieve the callback method on StatusCallback class
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");
					// call the callback on the statusObj we got in param when calling create_user
					statusObj.Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				});
	}

	void encrypt(jni::JNIEnv &env,  const jni::String &jlocalDeviceId,  const jni::String &jrecipientUserId, jni::Array<jni::Object<jRecipientData>> &jrecipients,
			jni::Array<jni::jbyte> &jplainMessage,
			jni::Object<jLimeOutputBuffer> &jcipherMessage,
			jni::Object<jStatusCallback> &statusObj,
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

		m_manager->encrypt(jni::Make<std::string>(env, jlocalDeviceId),
			recipientUserId,
			recipients,
			plainMessage,
			cipherMessage,
			[c_vm, &statusObj, &jrecipients, &jcipherMessage, recipients, cipherMessage] (const lime::CallbackReturn status, const std::string message) {
				// get the env from VM
				jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};

				// access to java RecipientData fields
				auto RecipientDataClass = jni::Class<jRecipientData>::Find(g_env);
				auto RecipientDataPeerStatusField = RecipientDataClass.GetField<jni::jint>(g_env, "peerStatus");
				auto RecipientDataDRmessageField = RecipientDataClass.GetField<jni::Array<jni::jbyte>>(g_env, "DRmessage");

				// retrieve the cpp recipients vector and copy back to the jrecipients the peerStatus and DRmessage(if any)
				for (size_t i=0; i<recipients->size(); i++) {
					auto jrecipient = jrecipients.Get(g_env, i); // recipient is the recipientData javaObject
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
				jcipherMessage.Set(g_env, LimeOutputBufferField, jcipherMessageArray);

				// retrieve the callback method on StatusCallback class
				auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
				auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");

				// call the callback on the statusObj we got in param when calling create_user
				statusObj.Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
			},
			j2cEncryptionPolicy(encryptionPolicy)
		);
	}

	jni::jint decrypt(jni::JNIEnv &env,  const jni::String &jlocalDeviceId,  const jni::String &jrecipientUserId, const jni::String &jsenderDeviceId,
			jni::Array<jni::jbyte> &jDRmessage,
			jni::Array<jni::jbyte> &jcipherMessage,
			jni::Object<jLimeOutputBuffer> &jplainMessage) {

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

	void update(jni::JNIEnv &env, jni::Object<jStatusCallback> &statusObj, jni::jint jOPkServerLowLimit, jni::jint jOPkBatchSize) {
		JavaVM *c_vm;
		env.GetJavaVM(&c_vm);

		m_manager->update([c_vm, &statusObj] (const lime::CallbackReturn status, const std::string message)
				{
					// get the env from VM
					jni::JNIEnv& g_env { jni::GetEnv(*c_vm)};

					// retrieve the callback method on StatusCallback class
					auto StatusClass = jni::Class<jStatusCallback>::Find(g_env);
					auto StatusMethod = StatusClass.GetMethod<void (jni::jint, jni::String)>(g_env, "callback");

					// call the callback on the statusObj we got in param when calling create_user
					statusObj.Call(g_env, StatusMethod, c2jCallbackReturn(status), jni::Make<jni::String>(g_env, message));
				},
				jOPkServerLowLimit, jOPkBatchSize);
	}
};

#define METHOD(MethodPtr, name) jni::MakeNativePeerMethod<decltype(MethodPtr), (MethodPtr)>(name)

jni::RegisterNativePeer<jLimeManager>(env, jni::Class<jLimeManager>::Find(env), "peer",
	jni::MakePeer<jLimeManager, jni::String &>,
	"initialize",
	"finalize",
	METHOD(&jLimeManager::create_user, "n_create_user"),
	METHOD(&jLimeManager::delete_user, "delete_user"),
	METHOD(&jLimeManager::encrypt, "n_encrypt"),
	METHOD(&jLimeManager::decrypt, "n_decrypt"),
	METHOD(&jLimeManager::update, "n_update")
	);

return jni::Unwrap(jni::jni_version_1_2);
} // JNI_OnLoad
