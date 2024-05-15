/*
	lime_x3dh_protocol.cpp
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

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_x3dh_protocol.hpp"
#include "lime_settings.hpp"
#include "lime_impl.hpp"

#include "bctoolbox/exception.hh"

#include <iostream> // ostreamstring to generate incoming/outgoing messages debug trace
#include <iomanip>
#include <mutex>


using namespace::std;
using namespace::lime;

namespace lime {
	/** @brief Group in this namespace all the functions related to building or parsing x3dh packets
	 * @par Protocol Version 0x01:
	 *	Header is : Protccol Version Number<1 byte> || Message type<1 byte> || Curve id<1 byte>
	 *	Messages are : header<3 bytes> || Message content
	 *
	 *	If not an error the server responds with a message holding just a header of the same message type
	 *	except for getPeerBundle which shall be answered with a peerBundle message
	 *
	 *	Message types description :
	 *		- registerUser : Identity Key<EDDSA Public Key length>\n
	 *				SPk< ECDH Public key length > ||\n
	 *				SPk Signature< Signature Length > ||\n
	 *				SPk Id < 4 bytes>\n
	 *				OPk Keys Count<2 bytes unsigned integer Big endian> ||\n
	 *				( OPk< ECDH Public key length > || OPk Id <4 bytes>){Keys Count}
	 *
	 *		- deleteUser : empty message, user to delete is retrieved from header From
	 *
	 *		- postSPk :	SPk< ECDH Public key length > ||
	 *				SPk Signature< Signature Length > ||
	 *				SPk Id < 4 bytes>
	 *
	 *		- postOPks : 	Keys Count<2 bytes unsigned integer Big endian> ||\n
	 *				( OPk< ECDH Public key length > || OPk Id <4 bytes>){Keys Count}
	 *
	 *		- getPeerBundle : request Count < 2 bytes unsigned Big Endian> ||\n
	 *				(userId Size <2 bytes unsigned Big Endian> || UserId <...> (the GRUU of user we wan't to send a message)) {request Count}
	 *
	 *		- peerBundle :	bundle Count < 2 bytes unsigned Big Endian> ||\n
	 *				(   deviceId Size < 2 bytes unsigned Big Endian > || deviceId
	 *				    Flag<1 byte: 0 if no OPK in bundle, 1 if OPk is the bundle, 2 if no key bundle is associated to this device> ||
	 *				    Ik < EDDSA Public Key Length > ||
	 *				    SPk < ECDH Public Key Length > || SPK id <4 bytes>
	 *				    SPk_sig < Signature Length > ||
	 *				    (OPk < ECDH Public Key Length > || OPk id <4 bytes>){0,1 in accordance to flag}
	 *				) { bundle Count}
	 *
	 *		- getSelfOPks : empty message, ask server for the OPk Id it still holds for us
	 *
	 *		- selfOPks : OPk Count <2 bytes unsigned integer Big Endian> ||\n
	 *				(OPk id <4 bytes uint32_t big endian>){OPk Count}
	 *
	 *		- error :	errorCode<1 byte> || (errorMessage<...>){0,1}
	 */
	namespace x3dh_protocol {

		constexpr uint8_t X3DH_protocolVersion = 0x01;
		constexpr size_t X3DH_headerSize = 3;

		/* X3DH protocol messages builds */
		/**
		 * @brief Helper function to get human readable trace of x3dh messages types
		 *
		 * @param[in]	message_type	The message type in enum class format
		 *
		 * @return	the message type as a human readable string
		 */
		static std::string x3dh_messageTypeString(const x3dh_message_type message_type) {
			switch (message_type) {
				case x3dh_message_type::deprecated_registerUser :
					return "deprecated_registerUser";
				case x3dh_message_type::registerUser :
					return "registerUser";
				case x3dh_message_type::deleteUser :
					return "deleteUser";
				case x3dh_message_type::postSPk :
					return "postSPk";
				case x3dh_message_type::postOPks :
					return "postOPks";
				case x3dh_message_type::getPeerBundle :
					return "getPeerBundle";
				case x3dh_message_type::peerBundle :
					return "peerBundle";
				case x3dh_message_type::getSelfOPks :
					return "getSelfOPks";
				case x3dh_message_type::selfOPks :
					return "selfOPks";
				case x3dh_message_type::error :
					return "error";
			}
			LIME_LOGE<<"Internal Error: X3DH message type not part of its own enumeration, the compiler shall have spotted this";
			return "inconsistent"; // to make compiler happy, there is no reason to end here actually
		}

		/**
		 * @brief Build X3DH message header using current protocol Version byte
		 *
		 * @param[in]	message_type	The message type we are creating
		 * @param[in]	curve		The curve Id we're working with
		 *
		 * @return	a vector holding the well formed header ready to be expanded to include the message body
		 */
		static std::vector<uint8_t> X3DH_makeHeader(const x3dh_message_type message_type, const lime::CurveId curve) noexcept{
			LIME_LOGI<<hex<<setfill('0')<<"Build outgoing X3DH message:"<<endl
				<<"    Protocol Version is 0x"<<setw(2)<<static_cast<unsigned int>(X3DH_protocolVersion)<<endl
				<<"    Message Type is "<<x3dh_messageTypeString(message_type)<<" (0x"<<setw(2)<<static_cast<unsigned int>(message_type)<<")"<<endl
				<<"    CurveId is 0x"<<setw(2)<<static_cast<unsigned int>(curve);
			return std::vector<uint8_t> {X3DH_protocolVersion, static_cast<uint8_t>(message_type), static_cast<uint8_t>(curve)};
		}

		/**
		 * @brief build a registerUser message : Identity Key<EDDSA Public Key length>
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 * @param[in]		Ik		Self public identity key (formatted for signature algorithm)
		 * @param[in]		SPk		signed pre-key
		 * @param[in]		OPks		Vector of one time pre-keys
		 */
		template <typename Curve>
		void buildMessage_registerUser(std::vector<uint8_t> &message, const DSA<typename Curve::EC, lime::DSAtype::publicKey> &Ik, const SignedPreKey<Curve> &SPk, const std::vector<OneTimePreKey<Curve>> &OPks) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::registerUser, Curve::curveId());
			// append the Ik
			message.insert(message.end(), Ik.cbegin(), Ik.cend());
			// append SPk
			auto serialSPk = SPk.serializePublic();
			message.insert(message.end(), serialSPk.cbegin(), serialSPk.cend());

			// check we do not try to upload more than 2^16 OPks as the counter is on 2 bytes
			auto OPkCount = OPks.size();
			if (OPkCount > 0xFFFF) {
				OPkCount = 0xFFFF;
				LIME_LOGW << "Trying to publish "<<static_cast<unsigned int>(OPks.size())<<" OPks wich is more than the maximum allowed. Actually publish the first 2^16 and discard the rest";
			}

			// append OPks number and a sequence of OPk || OPk_id
			message.push_back(static_cast<uint8_t>(((OPkCount)>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((OPkCount)&0xFF));

			// debug trace
			ostringstream message_trace;
			message_trace << hex << setfill('0') << "Outgoing X3DH registerUser message holds:"<<endl<<"    Ik: ";
			hexStr(message_trace, Ik.data(), DSA<Curve, lime::DSAtype::publicKey>::ssize());

			SPk.dump(message_trace, "    ");
			message_trace << endl << dec << setfill('0') << "    " << static_cast<unsigned int>(OPkCount)<<" OPks."<< hex;

			for (const auto &OPk : OPks) {
				auto serializedOPk = OPk.serializePublic();
				message.insert(message.end(), serializedOPk.cbegin(), serializedOPk.cend());
				// debug trace
				OPk.dump(message_trace);
			}

			LIME_LOGI<<message_trace.str();
		}

		/**
		 * @brief build a deleteUser message
		 *
		 * 	empty message, server retrieves deviceId to delete from authentication header, you cannot delete someone else!
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 */
		template <typename Curve>
		void buildMessage_deleteUser(std::vector<uint8_t> &message) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::deleteUser, Curve::curveId());
		}


		/**
		 * @brief build a postSPk message
		 *
		 *		SPk< ECDH Public key length > ||
		 *		SPk Signature< Signature Length > ||
		 *		SPk Id < 4 bytes>
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 * @param[in]		SPk		Signed Pre-Key
		 */
		template <typename Curve>
		void buildMessage_publishSPk(std::vector<uint8_t> &message, const SignedPreKey<Curve> &SPk) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::postSPk, Curve::curveId());
			// append SPk, Signature and SPkId
			auto serialSPk = SPk.serializePublic();
			message.insert(message.end(), serialSPk.cbegin(), serialSPk.cend());

			// debug trace
			ostringstream message_trace;
			message_trace << hex << setfill('0') << "Outgoing X3DH postSPk message holds:";
			SPk.dump(message_trace);
			LIME_LOGI<<message_trace.str();
		}

		/**
		 * @brief build a postOPks message
		 *
		 * 		Keys Count<2 bytes unsigned integer Big endian> ||\n
		 * 		( OPk< ECDH Public key length > ||
		 * 		OPk Id <4 bytes>){Keys Count}
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 * @param[in]		OPks		a vector of OPks to be published
		 */
		template <typename Curve>
		void buildMessage_publishOPks(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<Curve>> &OPks) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::postOPks, Curve::curveId());

			auto OPkCount = OPks.size();

			// check we do not try to upload more than 2^16 OPks as the counter is on 2 bytes
			if (OPkCount > 0xFFFF) {
				OPkCount = 0xFFFF;
				LIME_LOGW << "Trying to publish "<<static_cast<unsigned int>(OPks.size())<<" OPks wich is more than the maximum allowed. Actually publish the first 2^!6 and discard the rest";
			}

			// append OPks number and a sequence of OPk || OPk_id
			message.push_back(static_cast<uint8_t>(((OPkCount)>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((OPkCount)&0xFF));

			// debug trace
			ostringstream message_trace;
			message_trace << dec << setfill('0') << "Outgoing X3DH postOPks message holds "<< static_cast<unsigned int>(OPkCount)<<" OPks."<< hex;

			for (const auto &OPk : OPks) {
				auto serializedOPk = OPk.serializePublic();
				message.insert(message.end(), serializedOPk.cbegin(), serializedOPk.cend());

				// debug trace
				OPk.dump(message_trace);
			}

			//debug trace
			LIME_LOGI<<message_trace.str();
		}

		/**
		 * @brief build a getPeerBundle message
		 *
		 * 		request Count < 2 bytes unsigned Big Endian> ||\n
		 * 		(userId Size <2 bytes unsigned Big Endian> ||
		 * 		UserId <...> (the GRUU of user we wan't to send a message)) {request Count}
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 * @param[in]	peer_device_ids	a vector of all devices id for wich we request a key bundle
		 */
		template <typename Curve>
		void buildMessage_getPeerBundles(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::getPeerBundle, Curve::curveId());

			// append peer number
			message.push_back(static_cast<uint8_t>(((peer_device_ids.size())>>8)&0xFF));
			message.push_back(static_cast<uint8_t>((peer_device_ids.size())&0xFF));

			if (peer_device_ids.size()>0xFFFF) { // we're asking for more than 2^16 key bundles, really?
				LIME_LOGW<<"We are about to request for more than 2^16 key bundles to the X3DH server, it won't fit in protocol, truncate the request to 2^16 but it's very very unusual";
				peer_device_ids.resize(0xFFFF); // resize to max possible value
			}

			// debug trace
			ostringstream message_trace;
			message_trace << dec << setfill('0') << "Outgoing X3DH getPeerBundles message holds "<< static_cast<unsigned int>(peer_device_ids.size())<<" devices id."<< hex;

			// append a sequence of peer device Id size(on 2 bytes) || device id
			for (const auto &peer_device_id : peer_device_ids) {
				message.push_back(static_cast<uint8_t>(((peer_device_id.size())>>8)&0xFF));
				message.push_back(static_cast<uint8_t>((peer_device_id.size())&0xFF));
				message.insert(message.end(),peer_device_id.cbegin(), peer_device_id.cend());
				LIME_LOGI<<"Request X3DH keys for device "<<peer_device_id;

				// debug trace
				message_trace << endl << dec <<"    Device id("<< static_cast<unsigned int>(peer_device_id.size())<<"bytes): "<<peer_device_id<<" HEX:"<<hex;
				std::for_each(peer_device_id.cbegin(), peer_device_id.cend(), [&message_trace] (unsigned int i) {
					message_trace << setw(2) << i << ", ";
				});
			}

			//debug trace
			LIME_LOGI<<message_trace.str();
		}

		/**
		 * @brief build a  getSelfOPks message
		 *
		 * 	empty message, ask server for the OPk Id it still holds for us
		 *
		 * @param[in,out]	message		an empty buffer to store the message
		 */
		template <typename Curve>
		void buildMessage_getSelfOPks(std::vector<uint8_t> &message) noexcept {
			// create the header
			message = X3DH_makeHeader(x3dh_message_type::getSelfOPks, Curve::curveId());
		}


		/*
		 * @brief Perform validity verifications on x3dh message and extract its type and error code if its the case
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[out]	message_type	the message type
		 * @param[out]	error_code	the error code, unchanged if the message type is not error
		 * @param[in]	callback	in case of error, directly call it giving a meaningfull error message
		 *
		 * @return	true if message is well formed(check performed mostly on header value, correct header but not well formed message body are not detected at this point)
		 */
		template <typename Curve>
		bool parseMessage_getType(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept {
			// Trace incoming message parsing their content to display human readable trace
			ostringstream message_trace;
			message_trace << hex << setfill('0') << "Incoming X3DH message: "<<endl;
			hexStr(message_trace, body.data(), body.size(), 7);

			// check message holds at leat a header before trying to read it
			if (body.size()<X3DH_headerSize) {
				LIME_LOGE<<"Got an invalid response from X3DH server"<<endl<< message_trace.str()<<endl<<"    Invalid Incoming X3DH message";
				if (callback) callback(lime::CallbackReturn::fail, "Got an invalid response from X3DH server");
				LIME_LOGE<<message_trace.str()<<endl;
				return false;
			}

			// check X3DH protocol version
			if (body[0] != static_cast<uint8_t>(X3DH_protocolVersion)) {
				LIME_LOGE<<"X3DH server runs an other version of X3DH protocol(server "<<static_cast<unsigned int>(body[0])<<" - local "<<static_cast<unsigned int>(X3DH_protocolVersion)<<")"<<endl<<message_trace.str()<<endl<<"    Invalid Incoming X3DH message";
				if (callback) callback(lime::CallbackReturn::fail, "X3DH server and client protocol version mismatch");
				LIME_LOGE<<message_trace.str()<<endl;
				return false;
			}

			// check curve id
			if (body[2] != static_cast<uint8_t>(Curve::curveId())) {
				LIME_LOGE<<"X3DH server runs curve Id "<<static_cast<unsigned int>(body[2])<<" while local is set to "<<static_cast<unsigned int>(Curve::curveId())<<" for this server)"<<endl<<message_trace.str()<<endl<<"    Invalid Incoming X3DH message";
				if (callback) callback(lime::CallbackReturn::fail, "X3DH server and client curve Id mismatch");
				return false;
			}

			// message trace: add the protocol version, message type is appended in the switch
			message_trace<<endl<<"    Protocol Version is 0x"<<setw(2)<<static_cast<unsigned int>(body[0])<<endl;

			// retrieve message_type from body[1]
			switch (static_cast<uint8_t>(body[1])) {
				case static_cast<uint8_t>(x3dh_message_type::registerUser) :
					message_type = x3dh_message_type::registerUser;
					message_trace<<"    Message Type is registerUser (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::registerUser)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::deleteUser) :
					message_type = x3dh_message_type::deleteUser;
					message_trace<<"    Message Type is deleteUser (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::deleteUser)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::postSPk) :
					message_type = x3dh_message_type::postSPk;
					message_trace<<"    Message Type is postSPk (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::postSPk)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::postOPks) :
					message_type = x3dh_message_type::postOPks;
					message_trace<<"    Message Type is postOPks (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::postOPks)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::getPeerBundle) :
					message_type = x3dh_message_type::getPeerBundle;
					message_trace<<"    Message Type is getPeerBundle (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::getPeerBundle)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::peerBundle) :
					message_type = x3dh_message_type::peerBundle;
					message_trace<<"    Message Type is peerBundle (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::peerBundle)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::getSelfOPks) :
					message_type = x3dh_message_type::getSelfOPks;
					message_trace<<"    Message Type is getSelfOPks (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::getSelfOPks)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::selfOPks) :
					message_type = x3dh_message_type::selfOPks;
					message_trace<<"    Message Type is selfOPks (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::selfOPks)<<")";
					break;
				case static_cast<uint8_t>(x3dh_message_type::error) :
					message_type = x3dh_message_type::error;
					message_trace<<"    Message Type is error (0x"<<setw(2)<<static_cast<unsigned int>(x3dh_message_type::error)<<")";
					break;
				default: // unknown message type: invalid packet
					message_trace<<"    Message Type is Unknown (0x"<<setw(2)<<static_cast<unsigned int>(body[1])<<")"<<endl<<"    Invalid Incoming X3DH message";
					LIME_LOGE<<message_trace.str()<<endl;
					return false;
			}

			// message trace : display also the curve Id so we identify the 3 bytes of message header
			message_trace<<endl<<"    CurveId is 0x"<<setw(2)<<static_cast<unsigned int>(body[2])<<endl;

			// retrieve the error code if needed
			if (message_type == x3dh_message_type::error) {
				if (body.size()<X3DH_headerSize+1) { // error message contains at least 1 byte of error code + possible message
					LIME_LOGE<<message_trace.str()<<endl;
					return false;
				}

				if (body.size() == X3DH_headerSize+1) {
					LIME_LOGE<<"X3DH server respond error : code "<<static_cast<unsigned int>(body[X3DH_headerSize])<<" (no error message)";
				} else {
					LIME_LOGE<<"X3DH server respond error : code "<<static_cast<unsigned int>(body[X3DH_headerSize])<<" : "<<std::string(body.cbegin()+X3DH_headerSize+1, body.cend());
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
					case static_cast<uint8_t>(x3dh_error_code::server_failure):
						error_code = x3dh_error_code::server_failure;
						break;
					case static_cast<uint8_t>(x3dh_error_code::resource_limit_reached):
						error_code = x3dh_error_code::resource_limit_reached;
						break;
					default: // unknown error code - could be an updated server
						LIME_LOGW<<"Receive unknown error code in X3DH message"<< static_cast<uint8_t>(body[X3DH_headerSize]);
						error_code = x3dh_error_code::unknown_error_code;
				}
			}
			LIME_LOGI<<message_trace.str()<<endl<<"    Valid Incoming X3DH message";

			return true;
		}

		/**
		 * @brief Parse a peerBundles message and populate a vector of peerBundles
		 *
		 * Warning: no checks are done on message type, they are performed before calling this function
		 *
		 * peerBundle :	bundle Count < 2 bytes unsigned Big Endian> ||\n
		 *	(   deviceId Size < 2 bytes unsigned Big Endian > || deviceId
		 *	    Flag<1 byte: 0 if no OPK in bundle, 1 if present, 2 no key bundle found on server> ||
		 *		   Ik < EDDSA Public Key Length > ||
		 *		   SPk < ECDH Public Key Length > || SPK id <4 bytes>
		 *		   SPk_sig < Signature Length > ||
		 *		   (OPk < ECDH Public Key Length > || OPk id <4 bytes>){0,1 in accordance to flag}
		 *	) { bundle Count}
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[out]	peersBundle	a vector to be populated from message content, is empty if none found
		 *
		 * @return true if all went ok, false and empty peersBundle otherwise
		 */
		template <typename Curve>
		bool parseMessage_getPeerBundles(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<Curve>> &peersBundle) noexcept {
			peersBundle.clear();
			if (body.size() < X3DH_headerSize+2) { // we must be able to at least have a count of bundles
				LIME_LOGE<<"Unable to parse content of X3DH peer Bundles message, body size is only "<<static_cast<unsigned int>(body.size());
				return false;
			}

			uint16_t peersBundleCount = (static_cast<uint16_t>(body[X3DH_headerSize]))<<8|body[X3DH_headerSize+1];

			size_t index = X3DH_headerSize+2;

			// message trace, display the incoming peer bundles in human readable format:
			// - number of key bundles in the message
			// -     device id
			// -        Ik
			// -        SPkid, SPk, SPk signature
			// -        OPkid OPk if any
			ostringstream message_trace;
			message_trace << dec << "X3DH Peer Bundles message holds "<<static_cast<unsigned int>(peersBundleCount)<<" key bundles"<<setfill('0');

			// loop on all expected bundles
			for (auto i=0; i<peersBundleCount; i++) {
				if (body.size() < index + 2) { // check we have at least a device size to read
					peersBundle.clear();
					LIME_LOGE<<"Invalid message: size is not what expected, cannot read device size, discard without parsing";
					LIME_LOGD<<"message_trace so far: "<<message_trace.str();
					return false;
				}

				// get device id (ASCII string)
				uint16_t deviceIdSize = (static_cast<uint16_t>(body[index]))<<8|body[index+1];
				index += 2;

				if (body.size() < index + deviceIdSize + 1) { // check we have at enough data to read: device size and the following flag
					peersBundle.clear();
					LIME_LOGE<<"Invalid message: size is not what expected, cannot read device id(size is"<<int(deviceIdSize)<<"), discard without parsing";
					LIME_LOGD<<"message_trace so far: "<<message_trace.str();
					return false;
				}
				std::string deviceId{body.cbegin()+index, body.cbegin()+index+deviceIdSize};
				index += deviceIdSize;

				// check if we have a key bundle and an OPk. Possible flag values: 0 no OPk, 1 OPk, 2 no key bundle at all
				// parse the key bundle flag
				lime::X3DHKeyBundleFlag keyBundleFlag = lime::X3DHKeyBundleFlag::noBundle;
				switch (body[index]) {
					case static_cast<uint8_t>(lime::X3DHKeyBundleFlag::noBundle):
						keyBundleFlag = lime::X3DHKeyBundleFlag::noBundle;
						break;
					case static_cast<uint8_t>(lime::X3DHKeyBundleFlag::noOPk):
						keyBundleFlag = lime::X3DHKeyBundleFlag::noOPk;
						break;
					case static_cast<uint8_t>(lime::X3DHKeyBundleFlag::OPk):
						keyBundleFlag = lime::X3DHKeyBundleFlag::OPk;
						break;
					default:
						LIME_LOGE<<"Invalid X3DH message: unexpected flag value "<<body[index]<<" in "<<deviceId<<" key bundle";
						LIME_LOGD<<"message_trace so far: "<<message_trace.str();
						peersBundle.clear();
						return false;
				}

				// if there is no bundle, just skip to the next one
				if (keyBundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
					// add device Id (and its size) to the trace
					message_trace << endl << dec << "    Device Id ("<<static_cast<unsigned int>(deviceIdSize)<<" bytes): "<<deviceId<<" has no key bundle"<<endl;
					peersBundle.emplace_back(std::move(deviceId));
					index += 1;
					continue; // skip to next one
				}

				bool haveOPk = (keyBundleFlag == lime::X3DHKeyBundleFlag::OPk);
				index += 1;

				// add device Id (and its size) and flag to the trace
				message_trace << endl << dec << "    Device Id ("<<static_cast<unsigned int>(deviceIdSize)<<" bytes): "<<deviceId<<(haveOPk?" has ":" does not have ")<<"OPk";

				if (body.size() < index + X3DH_peerBundle<Curve>::ssize(haveOPk)) {
					peersBundle.clear();
					LIME_LOGE<<"Invalid message: size is not what expected, not enough buffer to hold keys bundle, discard without parsing";
					LIME_LOGD<<"message_trace so far: "<<message_trace.str();
					return false;
				}

				peersBundle.emplace_back(std::move(deviceId), body.cbegin()+index, haveOPk, message_trace);
				index += X3DH_peerBundle<Curve>::ssize(haveOPk);
			}
			LIME_LOGI<<message_trace.str();
			return true;
		}

		/**
		 * @brief Parse a selfOPk message and populate a self OPk ids
		 *
		 * Warning: no checks are done on message type, they are performed before calling this function\n
		 * 	selfOPks : 	OPk Count <2 bytes unsigned integer Big Endian> ||\n
		 *			(OPk id <4 bytes uint32_t big endian>){OPk Count}
		 *
		 * @param[in]	body		a buffer holding the message
		 * @param[out]	selfOPkIds	a vector to be populated from message content, is empty if no OPk returned
		 *
		 * @return true if all went ok, false otherwise
		 */
		template <typename Curve>
		bool parseMessage_selfOPks(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept {
			selfOPkIds.clear();
			if (body.size() < X3DH_headerSize+2) { // we must be able to at least have a count of bundles

				return false;
			}

			uint16_t selfOPkIdsCount = (static_cast<uint16_t>(body[X3DH_headerSize]))<<8|body[X3DH_headerSize+1];

			if (body.size() < X3DH_headerSize+2+4*selfOPkIdsCount) { // this expected message size
				return false;
			}
			size_t index = X3DH_headerSize+2;

			// message trace, display the incoming self OPks in human readable format:
			// - number of OPks in the message
			// -        OPkid
			ostringstream message_trace;
			message_trace << dec << "X3DH self OPks message holds "<<static_cast<unsigned int>(selfOPkIdsCount)<<" OPk Ids"<<endl;
			message_trace << hex;

			// loop on all OPk Ids
			for (auto i=0; i<selfOPkIdsCount; i++) { // they are in big endian
				uint32_t OPk_id = static_cast<uint32_t>(body[index])<<24 |
						static_cast<uint32_t>(body[index+1])<<16 |
						static_cast<uint32_t>(body[index+2])<<8 |
						static_cast<uint32_t>(body[index+3]);
				index+=4;
				selfOPkIds.push_back(OPk_id);
				message_trace <<"    OPk Id: 0x"<< setw(8) << static_cast<unsigned int>(OPk_id)<<endl;
			}
			LIME_LOGI<<message_trace.str();
			return true;
		}


		/* Instanciate templated functions */
#ifdef EC25519_ENABLED
		template void buildMessage_registerUser<C255>(std::vector<uint8_t> &message, const DSA<C255::EC, lime::DSAtype::publicKey> &Ik, const SignedPreKey<C255> &SPk, const std::vector<OneTimePreKey<C255>> &OPks) noexcept;
		template void buildMessage_deleteUser<C255>(std::vector<uint8_t> &message) noexcept;
		template void buildMessage_publishSPk<C255>(std::vector<uint8_t> &message, const SignedPreKey<C255> &SPk) noexcept;
		template void buildMessage_publishOPks<C255>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C255>> &OPks) noexcept;
		template void buildMessage_getPeerBundles<C255>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		template void buildMessage_getSelfOPks<C255>(std::vector<uint8_t> &message) noexcept;
		template bool parseMessage_getType<C255>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		template bool parseMessage_getPeerBundles<C255>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C255>> &peersBundle) noexcept;
		template bool parseMessage_selfOPks<C255>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif

#ifdef EC448_ENABLED
		template void buildMessage_registerUser<C448>(std::vector<uint8_t> &message, const DSA<C448::EC, lime::DSAtype::publicKey> &Ik,  const SignedPreKey<C448> &SPk, const std::vector<OneTimePreKey<C448>> &OPks) noexcept;
		template void buildMessage_deleteUser<C448>(std::vector<uint8_t> &message) noexcept;
		template void buildMessage_publishSPk<C448>(std::vector<uint8_t> &message, const SignedPreKey<C448> &SPk) noexcept;
		template void buildMessage_publishOPks<C448>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C448>> &OPks) noexcept;
		template void buildMessage_getPeerBundles<C448>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		template void buildMessage_getSelfOPks<C448>(std::vector<uint8_t> &message) noexcept;
		template bool parseMessage_getType<C448>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		template bool parseMessage_getPeerBundles<C448>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C448>> &peersBundle) noexcept;
		template bool parseMessage_selfOPks<C448>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif
#ifdef HAVE_BCTBXPQ
		template void buildMessage_registerUser<C255K512>(std::vector<uint8_t> &message, const DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik,  const SignedPreKey<C255K512> &SPk, const std::vector<OneTimePreKey<C255K512>> &OPks) noexcept;
		template void buildMessage_deleteUser<C255K512>(std::vector<uint8_t> &message) noexcept;
		template void buildMessage_publishSPk<C255K512>(std::vector<uint8_t> &message, const SignedPreKey<C255K512> &SPk) noexcept;
		template void buildMessage_publishOPks<C255K512>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C255K512>> &OPks) noexcept;
		template void buildMessage_getPeerBundles<C255K512>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		template void buildMessage_getSelfOPks<C255K512>(std::vector<uint8_t> &message) noexcept;
		template bool parseMessage_getType<C255K512>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		template bool parseMessage_getPeerBundles<C255K512>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C255K512>> &peersBundle) noexcept;
		template bool parseMessage_selfOPks<C255K512>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif
	} //namespace x3dh_protocol
} //namespace lime
