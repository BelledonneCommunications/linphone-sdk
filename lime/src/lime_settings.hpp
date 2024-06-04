/*
	lime_settings.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

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

#ifndef lime_settings_hpp
#define lime_settings_hpp


namespace lime {
/** @brief Hold constants definition used as settings in all components of the lime library
 *
 * in lime_setting.hpp: you can tweak the behavior of the library.
 * No compatibility break between clients shall result by modifying this definitions
 * @note : you can tweak values but not the types, uint16_t values are intended to be bounded by 2^16 -1.
 *
 * in lime_defines.hpp: the constants defined cannot be modified without some work on the source code
 * unless you really know what you're doing, just leave them alone
 */
namespace settings {

/******************************************************************************/
/*                                                                            */
/* Double Ratchet related definitions                                         */
/*                                                                            */
/******************************************************************************/

	/** Each DR session stores a shared AD given at built and derived from Identity keys of sender and receiver\n
	 * SharedAD is computed by HKDF-Sha512(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
	 */
	constexpr size_t DRSessionSharedADSize=32;

	static_assert(DRSessionSharedADSize<64, "Shared AD is generated through HKDF-Sha512 with only one round implemented so its size can't be more than Sha512 max output size");

	/** Maximum number of Message we can skip(and store their keys) at reception of one message */
	constexpr uint16_t maxMessageSkip=512;

	/** after a message key is stored, count how many messages we can receive from peer before deleting the key at next update
	 * @note: implemented by receiving key chain, so any new skipped message in a chain will reset the counter to 0
	 */
	constexpr uint16_t maxMessagesReceivedAfterSkip = 64;

	/** @brief Maximum length of Sending chain
	 *
	 * when this count is reached without any return from peer
	 * the DR session is set to stale and we must create another one to send messages
	 * Can't be more than 2^16 as message number is send on 2 bytes
	 */
	constexpr uint16_t maxSendingChain=500;

	/** @brief KEM ratchet chain settings :
	 *
	 * - before KEMRatchetChainSize is reached (cummulative on sent and received messages), do not perform a KEM ratchet
	 * - previous setting is overriden if the last KEM  ratchet is older than maxKEMRatchetChainPeriod (in seconds)
	 */
	constexpr uint16_t KEMRatchetChainSize=42;
	constexpr unsigned int maxKEMRatchetChainPeriod=86400; // 1 day

	/** Lifetime of a session once not active anymore, unit is day */
	constexpr unsigned int DRSession_limboTime_days=30;

/******************************************************************************/
/*                                                                            */
/* X3DH related definitions                                                   */
/*                                                                            */
/******************************************************************************/
	/// in days, Life time of a signed pre-key, it will be set to stale after that period
	constexpr unsigned int SPK_lifeTime_days=7;
	/// in days, How long shall we keep a signed pre-key once it has been replaced by a new one
	constexpr unsigned int SPK_limboTime_days=30;

	// Note: the three following values can be overriden by call parameters when creating the user or calling update
	/// default batch size when uploading OPks to X3DH server
	constexpr uint16_t OPk_batchSize = 25;
	/// default batch size when creating a new user
	constexpr uint16_t OPk_initialBatchSize = 4*OPk_batchSize;
	/// default limit for keys on server to trigger generation/upload of a new batch of OPks
	constexpr uint16_t OPk_serverLowLimit = 100;
	/// in days, How long shall we keep an OPk in localStorage once we've noticed X3DH server dispatched it
	constexpr unsigned int OPk_limboTime_days=SPK_lifeTime_days+SPK_limboTime_days;
	/// in seconds, how often should we perform an update (check if we should publish new OPk, cleaning DB routine etc...)
	constexpr unsigned int OPk_updatePeriod=86400; // 1 day

} // namespace settings

} // namespace lime

#endif /* lime_settings_hpp */
