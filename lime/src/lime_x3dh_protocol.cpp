/*
	lime_x3dh_protocol.cpp
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
#include "lime_x3dh_protocol.hpp"
#include "lime_settings.hpp"
#include "lime_impl.hpp"

#include "belle-sip/belle-sip.h"
#include "bctoolbox/exception.hh"

using namespace::std;
using namespace::lime;

namespace lime {
	// Group in this namespace all the functions related to building or parsing x3dh packets
	namespace x3dh_protocol {
		/* Version 0x01:
		 *	Header is : Protccol Version Number<1 byte> || Message type<1 byte> || Curve id<1 byte>
		 *	Messages are : header<3 bytes> || Message content
		 *
		 *	If not an error the server responds with a message holding just a header of the same message type
		 *	except for getPeerBundle which shall be answered with a peerBundle message
		 *
		 *	Message types description :
		 *		- registerUser : Identity Key<EDDSA Public Key length>
		 *		- deleteUser : empty message, user to delete is retrieved from header From
		 *
		 *		- postSPk :	SPk<ECDH Public key length> ||
		 *				SPk Signature<Signature Length> ||
		 *				SPk Id < 4 bytes>
		 *
		 *		- postOPks : 	Keys Count<2 bytes unsigned integer Big endian> ||
		 *				( OPk<ECDH Public key length> || OPk Id <4 bytes>){Keys Count}
		 *
		 *		- getPeerBundle : request Count < 2 bytes unsigned Big Endian> ||
		 *				(userId Size <2 bytes unsigned Big Endian> || UserId <...> (the GRUU of user we wan't to send a message)) {request Count}
		 *
		 *		- peerBundle :	bundle Count < 2 bytes unsigned Big Endian> ||
		 *				(   Flag<1 byte: 0 if no OPK in bundle, 1 if present> ||
		 *				    Ik <EDDSA Public Key Length> ||
		 *				    SPk <ECDH Public Key Length> || SPK id <4 bytes>
		 *				    SPk_sig <Signature Length> ||
		 *				    (OPk <ECDH Public Key Length> || OPk id <4 bytes>){0,1 in accordance to flag}
		 *				) { bundle Count}
		 *
		 *		- getSelfOPks : empty message, ask server for the OPk Id it still holds for us
		 *
		 *		- selfOPks : OPk Count <2 bytes unsigned integer Big Endian> ||
		 *				(OPk id <4 bytes uint32_t big endian>){OPk Count}
		 *
		 *		- error :	errorCode<1 byte> || (errorMessage<...>){0,1}
		 */

		constexpr uint8_t X3DH_protocolVersion = 0x01;
		constexpr size_t X3DH_headerSize = 3;
		enum class x3dh_message_type : uint8_t{	unset_type=0x00,
							registerUser=0x01,
							deleteUser=0x02,
							postSPk=0x03,
							postOPks=0x04,
							getPeerBundle=0x05,
							peerBundle=0x06,
							getSelfOPks=0x07,
							selfOPks=0x08,
							error=0xff};

		enum class x3dh_error_code : uint8_t{	bad_content_type=0x00,
							bad_curve=0x01,
							missing_senderId=0x02,
							bad_x3dh_protocol_version=0x03,
							bad_size=0x04,
							user_already_in=0x05,
							user_not_found=0x06,
							db_error=0x07,
							bad_request=0x08,
							unset_error_code=0xff};
		/* X3DH protocol packets builds */
		static std::vector<uint8_t> X3DH_makeHeader(const x3dh_message_type message_type, const lime::CurveId curve) noexcept{
			return std::vector<uint8_t> {X3DH_protocolVersion, static_cast<uint8_t>(message_type), static_cast<uint8_t>(curve)};
		}

		// registerUser : Identity Key<EDDSA Public Key length>
		template <typename Curve>
		void buildMessage_registerUser(std::vector<uint8_t> &message, const ED<Curve> &Ik) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::registerUser, Curve::curveId());
			// append the Ik
			message.insert(message.end(), Ik.begin(), Ik.end());
		}

		// deleteUser : empty message, server retrieves deviceId to delete from authentication header, you cannot delete someone else!
		template <typename Curve>
		void buildMessage_deleteUser(std::vector<uint8_t> &message) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::deleteUser, Curve::curveId());
		}


		// postSPk :	SPk<ECDH Public key length> ||
		//		SPk Signature<Signature Length> ||
		//		SPk Id < 4 bytes>
		template <typename Curve>
		void buildMessage_publishSPk(std::vector<uint8_t> &message, const X<Curve> &SPk, const Signature<Curve> &Sig, const uint32_t SPk_id) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::postSPk, Curve::curveId());
			// append SPk, Signature and SPkId
			message.insert(message.end(), SPk.begin(), SPk.end());
			message.insert(message.end(), Sig.begin(), Sig.end());
			message.push_back(static_cast<uint8_t>((SPk_id>>24)&0xFF));
			message.push_back(static_cast<uint8_t>((SPk_id>>16)&0xFF));
			message.push_back(static_cast<uint8_t>((SPk_id>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((SPk_id)&0xFF));
		}

		// postOPks : 	Keys Count<2 bytes unsigned integer Big endian> ||
		//		( OPk<ECDH Public key length> || OPk Id <4 bytes>){Keys Count}
		template <typename Curve>
		void buildMessage_publishOPks(std::vector<uint8_t> &message, const std::vector<X<Curve>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::postOPks, Curve::curveId());

			auto OPkCount = OPks.size();
			// append OPks number and a sequence of OPk || OPk_id
			message.push_back(static_cast<uint8_t>(((OPkCount)>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((OPkCount)&0xFF));

			for (decltype(OPkCount) i=0; i<OPkCount; i++) {
				message.insert(message.end(), OPks[i].begin(), OPks[i].end());
				message.push_back(static_cast<uint8_t>((OPk_ids[i]>>24)&0xFF));
				message.push_back(static_cast<uint8_t>((OPk_ids[i]>>16)&0xFF));
				message.push_back(static_cast<uint8_t>((OPk_ids[i]>>8)&0xFF));
				message.push_back(static_cast<uint8_t>((OPk_ids[i])&0xFF));
			}
		}

		// getPeerBundle :	request Count < 2 bytes unsigned Big Endian> ||
		//			(userId Size <2 bytes unsigned Big Endian> || UserId <...> (the GRUU of user we wan't to send a message)) {request Count}
		template <typename Curve>
		void buildMessage_getPeerBundles(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::getPeerBundle, Curve::curveId());

			// append peer number
			message.push_back(static_cast<uint8_t>(((peer_device_ids.size())>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((peer_device_ids.size())&0xFF));

			if (peer_device_ids.size()>0xFFFF) { // we're asking for more than 2^16 key bundles, really?
				BCTBX_SLOGW<<"We are about to request for more than 2^16 key bundles to the X3DH server, it won't fit in protocol, truncate the request to 2^16 but it's very very unusual";
				peer_device_ids.resize(0xFFFF); // resize to max possible value
			}

			// append a sequence of peer device Id size(on 2 bytes) || device id
			for (auto &peer_device_id : peer_device_ids) {
				message.push_back(static_cast<uint8_t>(((peer_device_id.size())>>8)&0xFF));
				message.push_back(static_cast<uint8_t>((peer_device_id.size())&0xFF));
				message.insert(message.end(),peer_device_id.begin(), peer_device_id.end());
				BCTBX_SLOGI<<"Request X3DH keys for device "<<peer_device_id;
			}
		}

		// getSelfOPks : empty message, ask server for the OPk Id it still holds for us
		template <typename Curve>
		void buildMessage_getSelfOPks(std::vector<uint8_t> &message) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::getSelfOPks, Curve::curveId());
		}


		/*
		 * @brief Perform validity verifications on x3dh message and extract its type and error code if its the case
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[in]	bodySize	size of previous buffer
		 * @param[out]	message_type	the message type
		 * @param[out]	error_code	the error code, unchanged if the message type is not error
		 * @param[in]	callback	in case of error, directly call it giving a meaningfull error message
		 */
		template <typename Curve>
		bool parseMessage_getType(const uint8_t *body, const size_t bodySize, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept {
			// check message holds at leat a header before trying to read it
			if (body == nullptr || bodySize<X3DH_headerSize) {
				BCTBX_SLOGE<<"Got an invalid response from X3DH server";
				if (callback) callback(lime::callbackReturn::fail, "Got an invalid response from X3DH server");
				return false;
			}

			// check X3DH protocol version
			if (body[0] != static_cast<uint8_t>(X3DH_protocolVersion)) {
				BCTBX_SLOGE<<"X3DH server runs an other version of X3DH protocol(server "<<int(body[0])<<" - local "<<static_cast<uint8_t>(X3DH_protocolVersion)<<")";
				if (callback) callback(lime::callbackReturn::fail, "X3DH server and client protocol version mismatch");
				return false;
			}

			// check curve id
			if (body[2] != static_cast<uint8_t>(Curve::curveId())) {
				BCTBX_SLOGE<<"X3DH server runs curve Id "<<int(body[2])<<" while local is set to "<<static_cast<uint8_t>(Curve::curveId())<<" for this server)";
				if (callback) callback(lime::callbackReturn::fail, "X3DH server and client curve Id mismatch");
				return false;
			}

			// retrieve message_type from body[1]
			switch (static_cast<uint8_t>(body[1])) {
				case static_cast<uint8_t>(x3dh_message_type::registerUser) :
					message_type = x3dh_message_type::registerUser;
					break;
				case static_cast<uint8_t>(x3dh_message_type::deleteUser) :
					message_type = x3dh_message_type::deleteUser;
					break;
				case static_cast<uint8_t>(x3dh_message_type::postSPk) :
					message_type = x3dh_message_type::postSPk;
					break;
				case static_cast<uint8_t>(x3dh_message_type::postOPks) :
					message_type = x3dh_message_type::postOPks;
					break;
				case static_cast<uint8_t>(x3dh_message_type::getPeerBundle) :
					message_type = x3dh_message_type::getPeerBundle;
					break;
				case static_cast<uint8_t>(x3dh_message_type::peerBundle) :
					message_type = x3dh_message_type::peerBundle;
					break;
				case static_cast<uint8_t>(x3dh_message_type::getSelfOPks) :
					message_type = x3dh_message_type::getSelfOPks;
					break;
				case static_cast<uint8_t>(x3dh_message_type::selfOPks) :
					message_type = x3dh_message_type::selfOPks;
					break;
				case static_cast<uint8_t>(x3dh_message_type::error) :
					message_type = x3dh_message_type::error;
					break;
				default: // unknown message type: invalid packet
					return false;
			}

			// retrieve the error code if needed
			if (message_type == x3dh_message_type::error) {
				if (bodySize<X3DH_headerSize+1) { // error message contains at least 1 byte of error code + possible message
					return false;
				}

				if (bodySize==X3DH_headerSize+1) {
					BCTBX_SLOGE<<"X3DH server respond error : code "<<int(body[X3DH_headerSize])<<" (no error message)";
				} else {
					BCTBX_SLOGE<<"X3DH server respond error : code "<<int(body[X3DH_headerSize])<<" : "<<std::string(body+X3DH_headerSize+1, body+bodySize);
				}

				switch (static_cast<uint8_t>(body[X3DH_headerSize])) {
					case static_cast<uint8_t>(x3dh_error_code::bad_content_type):
						error_code = x3dh_error_code::bad_content_type;
						break;
					case static_cast<uint8_t>(x3dh_error_code::bad_curve):
						error_code = x3dh_error_code::bad_curve;
						break;
					case static_cast<uint8_t>(x3dh_error_code::missing_senderId):
						error_code = x3dh_error_code::missing_senderId;
						break;
					case static_cast<uint8_t>(x3dh_error_code::bad_x3dh_protocol_version):
						error_code = x3dh_error_code::bad_x3dh_protocol_version;
						break;
					case static_cast<uint8_t>(x3dh_error_code::bad_size):
						error_code = x3dh_error_code::bad_size;
						break;
					case static_cast<uint8_t>(x3dh_error_code::user_already_in):
						error_code = x3dh_error_code::user_already_in;
						break;
					case static_cast<uint8_t>(x3dh_error_code::user_not_found):
						error_code = x3dh_error_code::user_not_found;
						break;
					case static_cast<uint8_t>(x3dh_error_code::db_error):
						error_code = x3dh_error_code::db_error;
						break;
					case static_cast<uint8_t>(x3dh_error_code::bad_request):
						error_code = x3dh_error_code::bad_request;
						break;
					default: // unknown error code: invalid packet
						return false;
				}
			}
			return true;
		}

		/* peerBundle :	bundle Count < 2 bytes unsigned Big Endian> ||
		 *	(   deviceId Size < 2 bytes unsigned Big Endian > || deviceId
		 *	    Flag<1 byte: 0 if no OPK in bundle, 1 if present> ||
		 *		   Ik <EDDSA Public Key Length> ||
		 *		   SPk <ECDH Public Key Length> || SPK id <4 bytes>
		 *		   SPk_sig <Signature Length> ||
		 *		   (OPk <ECDH Public Key Length> || OPk id <4 bytes>){0,1 in accordance to flag}
		 *	) { bundle Count}
		 */

		/**
		 * @brief Parse a peerBundles message and populate a vector of peerBundles
		 * Warning: no checks are done on message type, they are performed before calling this function
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[in]	bodySize	size of previous buffer
		 * @param[out]	peersBundle	a vector to be populated from message content, is empty if none found
		 *
		 * @return true if all went ok, false and empty peersBundle otherwise
		 */
		template <typename Curve>
		bool parseMessage_getPeerBundles(const uint8_t *body, const size_t bodySize, std::vector<X3DH_peerBundle<Curve>> &peersBundle) noexcept {
			peersBundle.clear();
			if (bodySize < X3DH_headerSize+2) { // we must be able to at least have a count of bundles
				return false;
			}

			uint16_t peersBundleCount = (static_cast<uint16_t>(body[X3DH_headerSize]))<<8|body[X3DH_headerSize+1];
			size_t index = X3DH_headerSize+2;

			// loop on all expected bundles
			for (auto i=0; i<peersBundleCount; i++) {
				if (bodySize < index + 2) { // check we have at least a device size to read
					peersBundle.clear();
					return false;
				}

				// get device id (ASCII string)
				uint16_t deviceIdSize = (static_cast<uint16_t>(body[index]))<<8|body[index+1];
				index += 2;

				if (bodySize < index + deviceIdSize + 1) { // check we have at enough data to read: device size and the following OPk flag
					peersBundle.clear();
					return false;
				}
				std::string deviceId{body+index, body+index+deviceIdSize};
				index += deviceIdSize;

				// check if we have an OPk
				bool haveOPk = (body[index]==0)?false:true;
				index += 1;


				if (bodySize < index + ED<Curve>::keyLength() + X<Curve>::keyLength() + Signature<Curve>::signatureLength() + 4 + (haveOPk?(X<Curve>::keyLength()+4):0) ) {
					peersBundle.clear();
					return false;
				}

				// retrieve simple pointers to all keys and signature, the X3DH_peerBundle constructor will construct the keys out of them
				const uint8_t *Ik = body+index; index += ED<Curve>::keyLength();
				const uint8_t *SPk = body+index; index += X<Curve>::keyLength();
				uint32_t SPk_id = static_cast<uint32_t>(body[index])<<24 |
						static_cast<uint32_t>(body[index+1])<<16 |
						static_cast<uint32_t>(body[index+2])<<8 |
						static_cast<uint32_t>(body[index+3]);
				index += 4;
				const uint8_t *SPk_sig = body+index; index += Signature<Curve>::signatureLength();
				if (haveOPk) {
					const uint8_t *OPk = body+index; index += X<Curve>::keyLength();
					uint32_t OPk_id = static_cast<uint32_t>(body[index])<<24 |
						static_cast<uint32_t>(body[index+1])<<16 |
						static_cast<uint32_t>(body[index+2])<<8 |
						static_cast<uint32_t>(body[index+3]);
					index += 4;
					peersBundle.emplace_back(std::move(deviceId), Ik, SPk, SPk_id, SPk_sig, OPk, OPk_id);
				} else {
					peersBundle.emplace_back(std::move(deviceId), Ik, SPk, SPk_id, SPk_sig);
				}
			}
			return true;
		}

		/*
		 * selfOPks : OPk Count <2 bytes unsigned integer Big Endian> ||
		 *	(OPk id <4 bytes uint32_t big endian>){OPk Count}
		 */
		/**
		 * @brief Parse a selfPk message and populate a self OPk ids
		 * Warning: no checks are done on message type, they are performed before calling this function
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[in]	bodySize	size of previous buffer
		 * @param[out]	selfOPkIds	a vector to be populated from message content, is empty if no OPk returned
		 *
		 * @return true if all went ok, false otherwise
		 */
		template <typename Curve>
		bool parseMessage_selfOPks(const uint8_t *body, const size_t bodySize, std::vector<uint32_t> &selfOPkIds) noexcept {
			selfOPkIds.clear();
			if (bodySize < X3DH_headerSize+2) { // we must be able to at least have a count of bundles
				return false;
			}

			uint16_t selfOPkIdsCount = (static_cast<uint16_t>(body[X3DH_headerSize]))<<8|body[X3DH_headerSize+1];

			if (bodySize < X3DH_headerSize+2+4*selfOPkIdsCount) { // this expected message size
				return false;
			}
			size_t index = X3DH_headerSize+2;
			// loop on all OPk Ids
			for (auto i=0; i<selfOPkIdsCount; i++) { // they are in big endian
				uint32_t OPkId = static_cast<uint32_t>(body[index])<<24 |
						static_cast<uint32_t>(body[index+1])<<16 |
						static_cast<uint32_t>(body[index+2])<<8 |
						static_cast<uint32_t>(body[index+3]);
				index+=4;
				selfOPkIds.push_back(OPkId);
			}
			return true;
		}


		/* Instanciate templated functions */
#ifdef EC25519_ENABLED
		template void buildMessage_registerUser<C255>(std::vector<uint8_t> &message, const ED<C255> &Ik) noexcept;
		template void buildMessage_deleteUser<C255>(std::vector<uint8_t> &message) noexcept;
		template void buildMessage_publishSPk<C255>(std::vector<uint8_t> &message, const X<C255> &SPk, const Signature<C255> &Sig, const uint32_t SPk_id) noexcept;
		template void buildMessage_publishOPks<C255>(std::vector<uint8_t> &message, const std::vector<X<C255>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		template void buildMessage_getPeerBundles<C255>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		template void buildMessage_getSelfOPks<C255>(std::vector<uint8_t> &message) noexcept;
#endif

#ifdef EC448_ENABLED
		template void buildMessage_registerUser<C448>(std::vector<uint8_t> &message, const ED<C448> &Ik) noexcept;
		template void buildMessage_deleteUser<C448>(std::vector<uint8_t> &message) noexcept;
		template void buildMessage_publishSPk<C448>(std::vector<uint8_t> &message, const X<C448> &SPk, const Signature<C448> &Sig, const uint32_t SPk_id) noexcept;
		template void buildMessage_publishOPks<C448>(std::vector<uint8_t> &message, const std::vector<X<C448>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		template void buildMessage_getPeerBundles<C448>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		template void buildMessage_getSelfOPks<C448>(std::vector<uint8_t> &message) noexcept;
#endif
	} //namespace x3dh_protocol

	/* Network related functions */
	static void on_progress(belle_sip_body_handler_t *bh, belle_sip_message_t *m, void *data, size_t offset, size_t total) {
	}

	static void process_response_header(void *data, const belle_http_response_event_t *event){
		if (event->response){
			//auto code=belle_http_response_get_status_code(event->response);
		}
	}

	/**
	 * @brief Clean user data in case of problem or when we're done, it also process the asynchronous encryption queue
	 *
	 * @param[in/out] userData	the structure holding the data passed through the bellesip callback
	 */
	template <typename Curve>
	void Lime<Curve>::cleanUserData(callbackUserData<Curve> *userData) {
		if (userData->plainMessage!=nullptr) { // only encryption request for X3DH bundle would populate the plainMessage field of user data structure
			// userData is actually a part of the Lime Object and allocated as a shared pointer, just set it to nullptr it will cleanly destroy it
			m_ongoing_encryption = nullptr;
			// check if others encryptions are in queue and call them if needed
			if (!m_encryption_queue.empty()) {
				auto userData = m_encryption_queue.front();
				m_encryption_queue.pop(); // remove it from queue and do it, as there is no more ongoing it shall be processed even if the queue still holds elements
				encrypt(userData->recipientUserId, userData->recipients, userData->plainMessage, userData->cipherMessage, userData->callback);
			}
		} else { // its not an encryption, user Data was generated through new, just delete it
			delete(userData);
		}
	}

	template <typename Curve>
	void Lime<Curve>::process_response(void *data, const belle_http_response_event_t *event) noexcept {
		if (event->response){
			auto code=belle_http_response_get_status_code(event->response);
			callbackUserData<Curve> *userData = static_cast<callbackUserData<Curve> *>(data);
			auto thiz = userData->limeObj.lock(); // get a shared pointer to Lime Object from the weak pointer stored in userData
			// check it is valid (lock() returns nullptr)
			if (!thiz) { // our Lime caller object doesn't exists anymore
				BCTBX_SLOGE<<"Got response from X3DH server but our Lime Object has been destroyed";
				delete(userData);
				return;
			}
			auto callback = userData->callback; // get callback

			if (code == 200) { // HTTP server is happy with our packet
				// check response from X3DH server: header shall be X3DH protocol version || message type || curveId
				belle_sip_message_t *message = BELLE_SIP_MESSAGE(event->response);
				// all raw data access functions in lime use uint8_t *, so safely cast the body pointer to it, it's just a data stream pointer anyway
				auto body = reinterpret_cast<const uint8_t *>(belle_sip_message_get_body(message));
				auto bodySize = belle_sip_message_get_body_size(message);

				lime::x3dh_protocol::x3dh_message_type message_type{x3dh_protocol::x3dh_message_type::unset_type};
				lime::x3dh_protocol::x3dh_error_code error_code{x3dh_protocol::x3dh_error_code::unset_error_code};
				// check message validity, extract type and error code(if any)
				if (!x3dh_protocol::parseMessage_getType<Curve>(body, bodySize, message_type, error_code, callback)) {
					thiz->cleanUserData(userData);
					return;
				}

				switch (message_type) {
					// for registerUser, deleteUser, postSPk and postOPks, on success, server will respond with an identical header
					// but we cannot get from server unset_type, getPeerBundle or getSelfOPks message
					case x3dh_protocol::x3dh_message_type::unset_type:
					case x3dh_protocol::x3dh_message_type::getPeerBundle:
					case x3dh_protocol::x3dh_message_type::getSelfOPks: {
						if (callback) callback(lime::callbackReturn::fail, "X3DH unexpected message from server");
						thiz->cleanUserData(userData);
					}
					return;

					case x3dh_protocol::x3dh_message_type::registerUser: {
						// server response to a registerUser, we shall have the network state machine set to next action: sendSPk, just check it
						if (userData->network_state_machine == lime::network_state::sendSPk) {
							userData->network_state_machine = lime::network_state::sendOPk;
							// generate and publish the SPk
							X<Curve> SPk{};
							Signature<Curve> SPk_sig{};
							uint32_t SPk_id=0;
							thiz->X3DH_generate_SPk(SPk, SPk_sig, SPk_id);
							std::vector<uint8_t> X3DHmessage{};
							x3dh_protocol::buildMessage_publishSPk(X3DHmessage, SPk, SPk_sig, SPk_id);
							thiz->postToX3DHServer(userData, X3DHmessage);
						} else { // after registering a user, we must post SPk and OPks
							if (callback) callback(lime::callbackReturn::fail, "Internal Error: we registered a new user on X3DH server but do not plan to post SPk or OPks");
							thiz->cleanUserData(userData);
						}
					}
					return;

					case x3dh_protocol::x3dh_message_type::postSPk: {
						// server response to a post SPk, if we are at user creation, the state machine is set to next action post OPk, do it
						if (userData->network_state_machine == lime::network_state::sendOPk) {
							userData->network_state_machine = lime::network_state::done;
							// generate and publish the OPks
							std::vector<X<Curve>> OPks{};
							std::vector<uint32_t> OPk_ids{};
							thiz->X3DH_generate_OPks(OPks, OPk_ids, userData->OPkBatchSize);
							std::vector<uint8_t> X3DHmessage{};
							x3dh_protocol::buildMessage_publishOPks(X3DHmessage, OPks, OPk_ids);
							thiz->postToX3DHServer(userData, X3DHmessage);
							return;
						}
					}
					break;

					case x3dh_protocol::x3dh_message_type::deleteUser:
					case x3dh_protocol::x3dh_message_type::postOPks:
						// server response to deleteUser or postOPks, nothing to do really
						// success callback is the common behavior, performed after the switch
					break;

					case x3dh_protocol::x3dh_message_type::peerBundle: {
						// server response to a getPeerBundle packet
						std::vector<X3DH_peerBundle<Curve>> peersBundle;
						if (!x3dh_protocol::parseMessage_getPeerBundles(body, bodySize, peersBundle)) { // parsing went wrong
							BCTBX_SLOGE<<"Got an invalid peerBundle packet from X3DH server";
							if (callback) callback(lime::callbackReturn::fail, "Got an invalid peerBundle packet from X3DH server");
							thiz->cleanUserData(userData);
							return;
						}

						// generate X3DH init packets, create a store DR Sessions(in Lime obj cache, they'll be stored in DB when the first encryption will occurs)
						try {
							//Note: if while we were waiting for the peer bundle we did get an init message from him and created a session
							// just do nothing : create a second session with the peer bundle we retrieved and at some point one session will stale
							// when message stop crossing themselves on the network
							thiz->X3DH_init_sender_session(peersBundle);
						} catch (BctbxException &e) { // something went wrong but we can't forward the exception to belle-sip, go for callback
							if (callback) callback(lime::callbackReturn::fail, std::string{"Error during the peer Bundle processing : "}.append(e.what()));
							thiz->cleanUserData(userData);
							return;
						}

						// call the encrypt function again, it will call the callback when done, encryption queue won't be processed as still locked by the m_ongoing_encryption member
						thiz->encrypt(userData->recipientUserId, userData->recipients, userData->plainMessage, userData->cipherMessage, callback);

						// now we can safely delete the user data, note that this may trigger an other encryption if there is one in queue
						thiz->cleanUserData(userData);
					}
					return;

					case x3dh_protocol::x3dh_message_type::selfOPks: {
						// server response to a getSelfOPks
						std::vector<uint32_t> selfOPkIds{};
						if (!x3dh_protocol::parseMessage_selfOPks<Curve>(body, bodySize, selfOPkIds)) { // parsing went wrong
							BCTBX_SLOGE<<"Got an invalid selfOPKs packet from X3DH server";
							if (callback) callback(lime::callbackReturn::fail, "Got an invalid selfOPKs packet from X3DH server");
							thiz->cleanUserData(userData);
							return;
						}

						// update in LocalStorage the OPk status: tag removed from server and delete old keys
						thiz->X3DH_updateOPkStatus(selfOPkIds);

						// Check if we shall upload more packets
						if (selfOPkIds.size() < userData->OPkServerLowLimit) {
							// generate and publish the OPks
							std::vector<X<Curve>> OPks{};
							std::vector<uint32_t> OPk_ids{};
							thiz->X3DH_generate_OPks(OPks, OPk_ids, userData->OPkBatchSize);
							std::vector<uint8_t> X3DHmessage{};
							x3dh_protocol::buildMessage_publishOPks(X3DHmessage, OPks, OPk_ids);
							thiz->postToX3DHServer(userData, X3DHmessage);
						} else { /* nothing to do, just call the callback */
							if (callback) callback(lime::callbackReturn::success, "");
							thiz->cleanUserData(userData);
						}
					}
					return;

					case x3dh_protocol::x3dh_message_type::error: {
						// error messages are logged inside the parseMessage_getType function, just return failure to callback
						if (callback) callback(lime::callbackReturn::fail, "X3DH server error");
						thiz->cleanUserData(userData);
					}
					return;
				}

				// we get here only if processing is over and response was the expected one
				if (callback) callback(lime::callbackReturn::success, "");
				thiz->cleanUserData(userData);
				return;

			} else { // response code is not 200Ok
				if (callback) callback(lime::callbackReturn::fail, std::string("Got a non Ok response from server : ").append(std::to_string(code)));
				thiz->cleanUserData(userData);
				return;
			}
		}
	}

	template <typename Curve>
	void Lime<Curve>::process_io_error(void *data, const belle_sip_io_error_event_t *event) noexcept{
		callbackUserData<Curve> *userData = static_cast<callbackUserData<Curve> *>(data);
		auto thiz = userData->limeObj.lock(); // get a shared pointer to Lime Object from the weak pointer stored in userData
		// check it is valid (lock() returns nullptr)
		if (!thiz) { // our Lime caller object doesn't exists anymore
			BCTBX_SLOGE<<"Unable to contact X3DH server and our Lime Object has been destroyed";
			delete(userData);
			return;
		}
		auto callback = userData->callback; // get callback

		if (callback) callback(lime::callbackReturn::fail, "Unable to contact X3DH server");
		thiz->cleanUserData(userData);
	}

	template <typename Curve>
	void Lime<Curve>::process_auth_requested(void *data, belle_sip_auth_event_t *event) noexcept{
		callbackUserData<Curve> *userData = static_cast<callbackUserData<Curve> *>(data);
		auto thiz = userData->limeObj.lock(); // get a shared pointer to Lime Object from the weak pointer stored in userData
		// check it is valid (lock() returns nullptr)
		if (!thiz) { // our Lime caller object doesn't exists anymore
			BCTBX_SLOGE<<"Unable to contact X3DH server and our Lime Object has been destroyed";
			delete(userData);
			return;
		}

		// Set in the auth the username of current user (X3DH uses device Id)
		belle_sip_auth_event_set_username(event, thiz->m_selfDeviceId.data());
		if (thiz->m_user_auth) thiz->m_user_auth(event);
	}


	template <typename Curve>
	void Lime<Curve>::postToX3DHServer(callbackUserData<Curve> *userData, const std::vector<uint8_t> &message) {
		belle_http_request_listener_callbacks_t cbs={};
		belle_http_request_listener_t *l;
		belle_generic_uri_t *uri;
		belle_http_request_t *req;
		belle_sip_memory_body_handler_t *bh;

		bh = belle_sip_memory_body_handler_new_copy_from_buffer(message.data(), message.size(), on_progress, NULL);

		uri=belle_generic_uri_parse(m_X3DH_Server_URL.data());

		req=belle_http_request_create("POST",
				uri,
				belle_http_header_create("User-Agent", "lime"),
				belle_http_header_create("Content-type", "x3dh/octet-stream"),
				belle_http_header_create("From", m_selfDeviceId.data()),
				NULL);

		belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req),BELLE_SIP_BODY_HANDLER(bh));
		cbs.process_response=Lime<Curve>::process_response;
		cbs.process_response_headers=process_response_header;
		cbs.process_io_error=Lime<Curve>::process_io_error;
		cbs.process_auth_requested=Lime<Curve>::process_auth_requested;
		l=belle_http_request_listener_create_from_callbacks(&cbs,static_cast<void *>(userData));
		belle_sip_object_data_set(BELLE_SIP_OBJECT(req), "http_request_listener", l, belle_sip_object_unref); // Ensure the listener object is destroyed when the request is destroyed
		belle_http_provider_send_request(m_http_provider,req,l);
	}


	/* X3DH Messages building functions */

	/* Instanciate templated member functions */
#ifdef EC25519_ENABLED
	template void Lime<C255>::postToX3DHServer(callbackUserData<C255> *userData, const std::vector<uint8_t> &message);
	template void Lime<C255>::process_response(void *data, const belle_http_response_event_t *event) noexcept;
	template void Lime<C255>::process_io_error(void *data, const belle_sip_io_error_event_t *event) noexcept;
	template void Lime<C255>::process_auth_requested(void *data, belle_sip_auth_event_t *event) noexcept;
	template void Lime<C255>::cleanUserData(callbackUserData<C255> *userData);
#endif

#ifdef EC448_ENABLED
	template void Lime<C448>::postToX3DHServer(callbackUserData<C448> *userData, const std::vector<uint8_t> &message);
	template void Lime<C448>::process_response(void *data, const belle_http_response_event_t *event) noexcept;
	template void Lime<C448>::process_io_error(void *data, const belle_sip_io_error_event_t *event) noexcept;
	template void Lime<C448>::process_auth_requested(void *data, belle_sip_auth_event_t *event) noexcept;
	template void Lime<C448>::cleanUserData(callbackUserData<C448> *userData);
#endif
} //namespace lime
