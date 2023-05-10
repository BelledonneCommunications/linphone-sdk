/**
	@file lime_ffi.cpp

	@author Johan Pascal

	@copyright	Copyright (C) 2018  Belledonne Communications SARL

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
#include "lime/lime_ffi.h"
#include "lime/lime.hpp"
#include "lime_log.hpp"
#include "bctoolbox/exception.hh"
#include "lime_settings.hpp"
#include "lime_defines.hpp"
#include "lime_double_ratchet_protocol.hpp"

extern "C" {

using namespace::std;
using namespace::lime;
/*****
 * Opaque structutes definitions: used to let the C side store and send back pointers to the C++ objects or closures
 *****/
/**
 * @brief encapsulate a unique pointer to limeManager in an opaque structure
 *
 */
struct lime_manager_struct {
	std::unique_ptr<LimeManager> context; /**< the encapsulated lime manager held in a unique ptr so we're sure to destroy it easily */
};

/**
 * @brief an opaque structure holding the lime cpp closure to be used to forward the X3DH server's response to lime
 */
struct lime_ffi_data_struct {
	const limeX3DHServerResponseProcess responseProcess; /**<  a callback to forward the response to lib lime */
	lime_ffi_data_struct(const limeX3DHServerResponseProcess &response) : responseProcess(response) {};
};

/*****
 * Local Helpers functions
 *****/
static lime::CurveId ffi2lime_CurveId(const enum lime_ffi_CurveId curve) {
	switch (curve) {
		case lime_ffi_CurveId_c25519:
			return lime::CurveId::c25519;
		case lime_ffi_CurveId_c448:
			return lime::CurveId::c448;
		default:
			return lime::CurveId::unset;
	}
}

static enum lime_ffi_CallbackReturn lime2ffi_CallbackReturn(const lime::CallbackReturn cbReturn) {
	switch (cbReturn) {
		case lime::CallbackReturn::success:
			return lime_ffi_CallbackReturn_success;
		case lime::CallbackReturn::fail:
		default :
			return lime_ffi_CallbackReturn_fail;
	}
}

static lime::EncryptionPolicy ffi2lime_EncryptionPolicy(const enum lime_ffi_EncryptionPolicy encryptionPolicy) {
	switch (encryptionPolicy) {
		case lime_ffi_EncryptionPolicy_DRMessage :
			return lime::EncryptionPolicy::DRMessage;
		case lime_ffi_EncryptionPolicy_cipherMessage :
			return lime::EncryptionPolicy::cipherMessage;
		case lime_ffi_EncryptionPolicy_optimizeUploadSize :
			return lime::EncryptionPolicy::optimizeUploadSize;
		case lime_ffi_EncryptionPolicy_optimizeGlobalBandwidth :
			return lime::EncryptionPolicy::optimizeGlobalBandwidth;
		default:
			return lime::EncryptionPolicy::optimizeUploadSize;
	}
}

static enum lime_ffi_PeerDeviceStatus lime2ffi_PeerDeviceStatus(const lime::PeerDeviceStatus status) {
	switch (status) {
		case lime::PeerDeviceStatus::untrusted :
			return lime_ffi_PeerDeviceStatus_untrusted;
			break;
		case lime::PeerDeviceStatus::trusted :
			return lime_ffi_PeerDeviceStatus_trusted;
			break;
		case lime::PeerDeviceStatus::unsafe :
			return lime_ffi_PeerDeviceStatus_unsafe;
			break;
		case lime::PeerDeviceStatus::unknown :
			return lime_ffi_PeerDeviceStatus_unknown;
			break;
		case lime::PeerDeviceStatus::fail :
		default:
			return lime_ffi_PeerDeviceStatus_fail;
			break;
	}
}

static lime::PeerDeviceStatus ffi2lime_PeerDeviceStatus(const enum lime_ffi_PeerDeviceStatus status) {
	switch (status) {
		case lime_ffi_PeerDeviceStatus_untrusted :
			return lime::PeerDeviceStatus::untrusted;
			break;
		case lime_ffi_PeerDeviceStatus_trusted :
			return lime::PeerDeviceStatus::trusted;
			break;
		case lime_ffi_PeerDeviceStatus_unsafe :
			return lime::PeerDeviceStatus::unsafe;
			break;
		case lime_ffi_PeerDeviceStatus_unknown :
			return lime::PeerDeviceStatus::unknown;
			break;
		case lime_ffi_PeerDeviceStatus_fail :
		default:
			return lime::PeerDeviceStatus::fail;
			break;
	}
}



/*****
 * API doxygen doc in lime_ff.h
 * All functions would just intercept any exception rising and return an error if there were
 *****/
int lime_ffi_processX3DHServerResponse(lime_ffi_data_t userData, const int code, const uint8_t *response, const size_t response_size) {
	try {
		// turn the response into a vector<uint8_t>
		auto body = std::vector<uint8_t>(response, response+response_size);
		// forward it to lime using the closure stored in the userData structure
		userData->responseProcess(code, body);
		// we're done with the userData, delete it to be sure to free the closure
		delete(userData);
	} catch (exception const &e) { // just catch anything
		LIME_LOGE<<"FFI failed to process X3DH server response: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	return LIME_FFI_SUCCESS;
}

int lime_ffi_manager_init(lime_manager_t * const manager, const char *db, const lime_ffi_X3DHServerPostData X3DH_post_data, void *userData) {

	// create a lambda to manage the X3DH post, it shall just tweak the arguments to match C capabilities and forward the request to the provided C X3DH_post_data function
	limeX3DHServerPostData X3DHServerPost([userData, X3DH_post_data](const std::string &url, const std::string &from, const std::vector<uint8_t> &message, const limeX3DHServerResponseProcess responseProcess) {

			// use a dedicated structure to store the closure given by lime to process the response
			// so the C code can hold it through a pointer to an opaque structure
			// this struct will be freed after the responseProcess has been called
			lime_ffi_data_t limeData = new lime_ffi_data_struct(responseProcess);

			X3DH_post_data(userData, // forward the userData pointer
					limeData, // pass our pointer, it will be given back to the lime_ffi_processX3DHServerResponse function
					url.data(), from.data(),
					message.data(), message.size());
			});

	try {
		// LimeManager object is encapsulated in an opaque structure so the C code can keep track of it and provide it back when needed
		// the struct is freed by the destroy function(which will also destroy the LimeManager object)
		*manager = new lime_manager_struct();
		(*manager)->context = std::unique_ptr<LimeManager>(new LimeManager(std::string(db), X3DHServerPost));
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to create the lime manager: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}

	return LIME_FFI_SUCCESS;
}

int lime_ffi_create_user(lime_manager_t manager, const char *localDeviceId, const char *x3dhServerUrl, const enum lime_ffi_CurveId curve, const uint16_t OPkInitialBatchSize,
		const lime_ffi_Callback callback, void *callbackUserData) {

	// just intercept the lime callback, convert the arguments to the correct types, add the userData and forward it to the C side
	limeCallback cb([callback, callbackUserData](const lime::CallbackReturn status, const std::string message){
				callback(callbackUserData, lime2ffi_CallbackReturn(status), message.data());
			});

	try {
		manager->context->create_user(std::string(localDeviceId), std::string(x3dhServerUrl), ffi2lime_CurveId(curve), OPkInitialBatchSize, cb);
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to create user: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to create user: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	return LIME_FFI_SUCCESS;
}

int lime_ffi_delete_user(lime_manager_t manager, const char *localDeviceId, const lime_ffi_Callback callback, void *callbackUserData) {

	// just intercept the lime callback, convert the arguments to the correct types, add the userData and forward it to the C side
	limeCallback cb([callback, callbackUserData](const lime::CallbackReturn status, const std::string message){
				callback(callbackUserData, lime2ffi_CallbackReturn(status), message.data());
			});

	try {
		manager->context->delete_user(std::string(localDeviceId), cb);
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to delete user: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to delete user: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	return LIME_FFI_SUCCESS;
}

int lime_ffi_is_user(lime_manager_t manager, const char *localDeviceId) {
	try {
		if (manager->context->is_user(std::string(localDeviceId))) {
			return LIME_FFI_SUCCESS;
		} else {
			return LIME_FFI_USER_NOT_FOUND;
		}
	} catch (exception const &e) { // catch anything (BctbxException are already taken care of by is_user, but other kind may arise)
		LIME_LOGE<<"FFI failed to delete user: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
}

int lime_ffi_encryptOutBuffersMaximumSize(const size_t plainMessageSize, const enum lime_ffi_CurveId curve, size_t *DRmessageSize, size_t *cipherMessageSize) {
	/* cipherMessage maximum size is plain message size + auth tag size */
	*cipherMessageSize = plainMessageSize + lime::settings::DRMessageAuthTagSize;

	/* DRmessage maximum size is :
	 * DRmessage header size + X3DH init size + MAX(plain message size, RandomSeed Size) + auth tag size */
	*DRmessageSize = std::max(plainMessageSize, lime::settings::DRrandomSeedSize) + lime::settings::DRMessageAuthTagSize;

	switch (curve) {
		case lime_ffi_CurveId_c25519:
#ifdef EC25519_ENABLED
			*DRmessageSize += lime::double_ratchet_protocol::headerSize<C255>() + lime::double_ratchet_protocol::X3DHinitSize<C255>(true);
#else /* EC25519_ENABLED */
			return LIME_FFI_INVALID_CURVE_ARGUMENT;
#endif /* EC25519_ENABLED */
			break;

		case lime_ffi_CurveId_c448:
#ifdef EC448_ENABLED
			*DRmessageSize += lime::double_ratchet_protocol::headerSize<C448>() + lime::double_ratchet_protocol::X3DHinitSize<C448>(true);
#else /* EC448_ENABLED */
			return LIME_FFI_INVALID_CURVE_ARGUMENT;
#endif /* EC448_ENBALED*/
			break;

		default :
			return LIME_FFI_INVALID_CURVE_ARGUMENT;
	}

	return LIME_FFI_SUCCESS;
}

int lime_ffi_encrypt(lime_manager_t manager, const char *localDeviceId,
		const uint8_t *const recipientUserId, const size_t recipientUserIdSize,
		lime_ffi_RecipientData_t *const recipients, const size_t recipientsSize,
		const uint8_t *const plainMessage, const size_t plainMessageSize,
		uint8_t *const cipherMessage, size_t *cipherMessageSize,
		const lime_ffi_Callback callback, void *callbackUserData,
		enum lime_ffi_EncryptionPolicy encryptionPolicy) {


	/* create shared of recipients and cipher message, ownership is then taken by lime during encrypt until we get into the callback */
	auto l_recipients = make_shared<std::vector<RecipientData>>();
	// copy the whole list of recipients to a std::vector<recipientData>
	for (size_t i=0; i<recipientsSize; i++) {
		l_recipients->emplace_back(recipients[i].deviceId);
		// also propagate fail status spotting this entry to be ignored by the encryption engine
		switch (recipients[i].peerStatus) {
			case lime_ffi_PeerDeviceStatus_fail :
				l_recipients->back().peerStatus = lime::PeerDeviceStatus::fail;
				break;
			default : // any other status would be anyway overwritten, so do not bother and let it be unknown as it default at construction
				break;
		}
	}
	auto l_cipherMessage = make_shared<std::vector<uint8_t>>();

	/* capture :
	 * - callback and its userData so we can forward the lime's callback to the C side
	 * - recipients, cipherMessage and cipherMessageSize so we can the copy the DRmessage and cipherMessage from lime's encryp output to them (recipients size is actually stored in l_recipients as they have the same size)
	 * - l_recipients and l_cipherMessage so we can access them in the callback to get the encryption's output
	 *
	 * Note: all of the captured variables are pointers, so we copy capture them and not get their reference
	 */
	limeCallback cb([callback, callbackUserData, recipients, cipherMessage, cipherMessageSize, l_recipients, l_cipherMessage](const lime::CallbackReturn status, const std::string message){
			// if we have room to copy back the cipher Message to the given C buffer, do it.
			if (*cipherMessageSize >= l_cipherMessage->size()) {
				std::copy_n(l_cipherMessage->begin(), l_cipherMessage->size(), cipherMessage);
				*cipherMessageSize = l_cipherMessage->size();
			} else {
				callback(callbackUserData, lime_ffi_CallbackReturn_fail, "cipherMessage buffer is too small to hold result");
			}

			size_t i=0;
			// loop on all l_recipients, the are matching the indices of recipients array as it was built from it
			for (const auto &l_recipient:(*l_recipients)) {
				// copy back the DR messages(with their sizes)
				if (l_recipient.DRmessage.size() <= recipients[i].DRmessageSize) {
					std::copy_n(l_recipient.DRmessage.begin(), l_recipient.DRmessage.size(), recipients[i].DRmessage);
					recipients[i].DRmessageSize = l_recipient.DRmessage.size();
				} else {
					callback(callbackUserData, lime_ffi_CallbackReturn_fail, "DRmessage buffer is too small to hold result");
					return;
				}
				// and the peer device status
				recipients[i].peerStatus = lime2ffi_PeerDeviceStatus(l_recipient.peerStatus);
				i++;
			}
			callback(callbackUserData, lime2ffi_CallbackReturn(status), message.data());
		});

	/* encrypts */
	try {
		manager->context->encrypt(std::string(localDeviceId), make_shared<std::vector<uint8_t>>(recipientUserId, recipientUserId+recipientUserIdSize), l_recipients, make_shared<std::vector<uint8_t>>(plainMessage, plainMessage+plainMessageSize), l_cipherMessage, cb, ffi2lime_EncryptionPolicy(encryptionPolicy));
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to encrypt: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to encrypt: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}

	return LIME_FFI_SUCCESS;
}

enum lime_ffi_PeerDeviceStatus lime_ffi_decrypt(lime_manager_t manager, const char *localDeviceId,
		const uint8_t *const recipientUserId, const size_t recipientUserIdSize,
		const char *senderDeviceId,
		const uint8_t *const DRmessage, const size_t DRmessageSize,
		const uint8_t *const cipherMessage, const size_t cipherMessageSize,
		uint8_t *const plainMessage, size_t *plainMessageSize) {

	try {
		std::vector<uint8_t> l_plainMessage{};
		auto ret = manager->context->decrypt(std::string(localDeviceId), std::vector<uint8_t>(recipientUserId, recipientUserId+recipientUserIdSize), std::string(senderDeviceId), std::vector<uint8_t>(DRmessage, DRmessage+DRmessageSize), std::vector<uint8_t>(cipherMessage, cipherMessage+cipherMessageSize), l_plainMessage);

		if (l_plainMessage.size()<=*plainMessageSize) {
			std::copy_n(l_plainMessage.data(), l_plainMessage.size(), plainMessage);
			*plainMessageSize = l_plainMessage.size();
		}
		return lime2ffi_PeerDeviceStatus(ret);
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to encrypt: "<<e.str();
		return lime_ffi_PeerDeviceStatus_fail;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to encrypt: "<<e.what();
		return lime_ffi_PeerDeviceStatus_fail;
	}
}


int lime_ffi_manager_destroy(lime_manager_t manager) {
	manager->context = nullptr;
	delete (manager);
	return LIME_FFI_SUCCESS;
}

int lime_ffi_get_selfIdentityKey(lime_manager_t manager, const char *localDeviceId, uint8_t *const Ik, size_t *IkSize) {
	try {
		std::vector<uint8_t> l_Ik{};
		manager->context->get_selfIdentityKey(std::string(localDeviceId), l_Ik);

		if (l_Ik.size() <= *IkSize) {
			std::copy_n(l_Ik.begin(), l_Ik.size(), Ik);
			*IkSize = l_Ik.size();
			return LIME_FFI_SUCCESS;
		} else {
			*IkSize = 0;
			return LIME_FFI_OUTPUT_BUFFER_TOO_SMALL;
		}
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to get self Identity key: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to get self Identity key: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
}

int lime_ffi_set_peerDeviceStatus(lime_manager_t manager, const char *peerDeviceId, const uint8_t *const Ik, const size_t IkSize, enum lime_ffi_PeerDeviceStatus status) {
	try {
		if (IkSize > 0) { // we have an Ik
			manager->context->set_peerDeviceStatus(std::string(peerDeviceId), std::vector<uint8_t>(Ik, Ik+IkSize), ffi2lime_PeerDeviceStatus(status));
		} else { // no Ik provided
			manager->context->set_peerDeviceStatus(std::string(peerDeviceId), ffi2lime_PeerDeviceStatus(status));
		}
		return LIME_FFI_SUCCESS;
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed to set self Identity key: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed to set self Identity key: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
}

enum lime_ffi_PeerDeviceStatus lime_ffi_get_peerDeviceStatus(lime_manager_t manager, const char *peerDeviceId) {
	return (lime2ffi_PeerDeviceStatus(manager->context->get_peerDeviceStatus(std::string(peerDeviceId))));
}

int lime_ffi_delete_peerDevice(lime_manager_t manager, const char *peerDeviceId) {
	manager->context->delete_peerDevice(std::string(peerDeviceId));
	return LIME_FFI_SUCCESS;
}

int lime_ffi_stale_sessions(lime_manager_t manager, const char *localDeviceId, const char *peerDeviceId) {
	manager->context->stale_sessions(std::string(localDeviceId), std::string(peerDeviceId));
	return LIME_FFI_SUCCESS;
}

int lime_ffi_update(lime_manager_t manager,  const char *localDeviceId, const lime_ffi_Callback callback, void *callbackUserData, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) {
	// just intercept the lime callback, convert the arguments to the correct types, add the userData and forward it to the C side
	limeCallback cb([callback, callbackUserData](const lime::CallbackReturn status, const std::string message){
				callback(callbackUserData, lime2ffi_CallbackReturn(status), message.data());
			});

	try {
		manager->context->update(std::string(localDeviceId), cb, OPkServerLowLimit, OPkBatchSize);
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed during update: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed during update: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	return LIME_FFI_SUCCESS;

}

int lime_ffi_set_x3dhServerUrl(lime_manager_t manager, const char *localDeviceId, const char *x3dhServerUrl) {
	try {
		manager->context->set_x3dhServerUrl(std::string(localDeviceId), std::string(x3dhServerUrl));
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed during set X3DH server Url: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed during set X3DH server Url: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	return LIME_FFI_SUCCESS;
}


int lime_ffi_get_x3dhServerUrl(lime_manager_t manager, const char *localDeviceId, char *x3dhServerUrl, size_t *x3dhServerUrlSize) {
	std::string url{};
	try {
		url = manager->context->get_x3dhServerUrl(std::string(localDeviceId));
	} catch (BctbxException const &e) {
		LIME_LOGE<<"FFI failed during get X3DH server Url: "<<e.str();
		return LIME_FFI_INTERNAL_ERROR;
	} catch (exception const &e) { // catch anything
		LIME_LOGE<<"FFI failed during get X3DH server Url: "<<e.what();
		return LIME_FFI_INTERNAL_ERROR;
	}
	// check the output buffer is large enough
	if (url.size() >= *x3dhServerUrlSize) { // >= as we need room for the NULL termination
		*x3dhServerUrlSize = 0;
		return LIME_FFI_OUTPUT_BUFFER_TOO_SMALL;
	} else {
		std::copy_n(url.begin(), url.size(), x3dhServerUrl);
		x3dhServerUrl[url.size()] = '\0';
		*x3dhServerUrlSize = url.size();
		return LIME_FFI_SUCCESS;
	}
}

} // extern "C"
