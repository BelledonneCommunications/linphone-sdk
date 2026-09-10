/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

#include <mediastreamer2/ice-session.h>

#include "conference/session/call-session.h"
#include "conference/session/media-description-renderer.h"
#include "nat/nat-policy.h"

using namespace ms2::nat;

LINPHONE_BEGIN_NAMESPACE

class StreamsGroup;
class MediaSessionPrivate;
class IceServiceListener;

class IceService : public MediaDescriptionRenderer {
public:
	explicit IceService(StreamsGroup &sg);
	~IceService() override;

	[[nodiscard]] bool isActive() const;

	/* Returns true if ICE has completed successfully. */
	[[nodiscard]] bool hasCompleted() const;

	/* Returns true if ICE is running. */
	[[nodiscard]] bool isRunning() const;

	/* Returns true if ICE has finished with the checklists processing, even if it has failed for some of the
	 * checklist.*/
	[[nodiscard]] bool hasCompletedCheckList() const;

	[[nodiscard]] bool isControlling() const;

	/* The ICE restart procedure as in RFC */
	void restartSession(IceRole role) const;

	/* Called after a network connectivity change, to restart ICE from the beginning.*/
	void resetSession() const;

	/* Returns true if the incoming offer requires a defered response, due to check-list(s) not yet completed.*/
	[[nodiscard]] bool reinviteNeedsDeferedResponse(const std::shared_ptr<SalMediaDescription> &remoteMd) const;

	void createStreams(const OfferAnswerContext &params);
	/**
	 * Called by the StreamsGroup when the local media description must be filled with ICE parameters.
	 *
	 */
	void fillLocalMediaDescription(OfferAnswerContext &ctx) override;
	/*
	 * Prepare to run.
	 * Returns true if operation is in progress, in which case StreamsGroup::finishPrepare() is to be called
	 * when operation has finally completed.
	 * Returns false is the prepare step is synchronously done.
	 */
	bool prepare() override;
	/*
	 * Prepare stage is finishing.
	 * Called by StreamsGroup's own finishPrepare() method.
	 *
	 */
	void finishPrepare() override;
	/*
	 * Render the streams according to offer answer context.
	 */
	void render(const OfferAnswerContext &ctx, CallSession::State targetState) override;
	/*
	 * Called to notify that the session is confirmed (corresponding to SIP ACK).
	 */
	void sessionConfirmed(const OfferAnswerContext &ctx) override;
	/*
	 * Stop rendering streams.
	 */
	void stop() override;
	/*
	 * Release engine's resource, pending object destruction.
	 */
	void finish() override;

	/*
	 * Set the listener to get notified of major ICE events. Used by the MediaSession to perform required signaling
	 * operations.
	 */
	void setListener(IceServiceListener *listener);

	/*
	 * Called by streams (who receive oRTP events) to notify ICE related events to the IceService.
	 * Ideally the IceService should place its own listener to these ortp events, but well oRTP is C and has to be
	 * simple.
	 */
	void handleIceEvent(const OrtpEvent *ev);

	/**
	 * used by non-regression tests only.
	 */
	[[nodiscard]] const std::shared_ptr<IceSession> &getSession() const {
		return mIceSession;
	}
	/**
	 * Check that the host has the "local network permission".
	 * This is useful for iOS. If not, ICE cannot be used because the socket API will refuse to send packets to a local
	 * address. In addition the source address specification (with sendmsg()/recvmsg() and control data block) does not
	 * work.
	 */
	[[nodiscard]] static bool hasLocalNetworkPermission(const std::list<std::string> &localAddrs);
	[[nodiscard]] static bool hasLocalNetworkPermission();

private:
	[[nodiscard]] static bool checkLocalNetworkPermission(const std::string &localAddr);
	[[nodiscard]] static bool iceFoundInMediaDescription(const std::shared_ptr<SalMediaDescription> &md);
	[[nodiscard]] static bool hasRelayCandidates(const SalMediaDescription &md);
	static void getIceDefaultAddrAndPort(uint16_t componentID,
	                                     const std::shared_ptr<SalMediaDescription> &md,
	                                     const SalStreamDescription &stream,
	                                     std::string &addr,
	                                     int &port);
	[[nodiscard]] static const struct addrinfo *getIcePreferredStunServerAddrinfo(const struct addrinfo *ai);

	[[nodiscard]] MediaSessionPrivate &getMediaSessionPrivate() const;
	[[nodiscard]] LinphoneCore *getCCore() const;
	void updateLocalMediaDescriptionFromIce(std::shared_ptr<SalMediaDescription> &desc) const;
	void clearUnusedIceCandidates(const std::shared_ptr<SalMediaDescription> &localDesc,
	                              const std::shared_ptr<SalMediaDescription> &remoteDesc,
	                              bool localIsOfferer) const;
	[[nodiscard]] bool checkForIceRestartAndSetRemoteCredentials(const std::shared_ptr<SalMediaDescription> &md,
	                                                             bool isOffer) const;
	void createIceCheckListsAndParseIceAttributes(const std::shared_ptr<SalMediaDescription> &md,
	                                              bool iceRestarted) const;
	void updateFromRemoteMediaDescription(const std::shared_ptr<SalMediaDescription> &localDesc,
	                                      const std::shared_ptr<SalMediaDescription> &remoteDesc,
	                                      bool isOffer);
	bool needIceGathering();
	void gatheringFinished();
	void deleteSession();
	void checkSession(IceRole role, bool preferIpv6DefaultCandidates);
	[[nodiscard]] int gatherIceCandidates();
	[[nodiscard]] int gatherSflrxIceCandidates(const struct addrinfo *stunServerAi);
	[[nodiscard]] int gatherLocalCandidates() const;
	void addPredefinedSflrxCandidates(const std::shared_ptr<NatPolicy> &natPolicy) const;
	void chooseDefaultCandidates(const OfferAnswerContext &ctx) const;
	void notifyEndOfPrepare();

	StreamsGroup &mStreamsGroup;
	std::shared_ptr<IceSession> mIceSession = nullptr;
	IceServiceListener *mListener = nullptr;
	NatPolicy::AsyncHandle mAsyncStunResolverHandle{};
	int mSflrxGatheringStatus = 0;
	bool mGatheringFinished = false;
	bool mAllowLateIce = false;
	bool mDontDefaultToStunCandidates = false;
	bool mEnableIntegrityCheck = true;
	bool mIceWasDisabled = false; // Remember that at some point ICE was disabled by an incoming offer or answer.
	bool mInsideGatherIceCandidates{};
};

class LINPHONE_INTERNAL_PUBLIC IceServiceListener {
public:
	virtual void onGatheringFinished(IceService &service) = 0;
	virtual void onIceCompleted(IceService &service) = 0;
	virtual void onLosingPairsCompleted(IceService &service) = 0;
	virtual void onIceRestartNeeded(IceService &service) = 0;
	virtual ~IceServiceListener() = default;
};

LINPHONE_END_NAMESPACE
