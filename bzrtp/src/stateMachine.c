/*
 * Copyright (c) 2014-2019 Belledonne Communications SARL.
 *
 * This file is part of bzrtp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include "packetParser.h"
#include "cryptoUtils.h"
#include "zidCache.h"
#include <bctoolbox/crypto.h>
#include "stateMachine.h"


/* Local functions prototypes */
int bzrtp_turnIntoResponder(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket, bzrtpCommitMessage_t *commitMessage);
int bzrtp_responseToHelloMessage(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket);
int bzrtp_computeS0DHMMode(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);
int bzrtp_computeS0MultiStreamMode(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);
int bzrtp_deriveKeysFromS0(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);
int bzrtp_deriveSrtpKeysFromS0(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);
int bzrtp_updateCachedSecrets(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);

/*
 * @brief This is the initial state
 * On first call, we will create the Hello message and start sending it until we receive an helloACK or a hello message from peer
 *
 * Arrives from :
 * 	- This is the initial state
 * Goes to:
 * 	- state_discovery_waitingForHello upon HelloACK reception
 * 	- state_discovery_waitingForHelloAck upon Hello reception
 * Send :
 * 	- Hello until timer's end or transition
 */
int state_discovery_init(bzrtpEvent_t event) {
	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;
	int retval;

	/*** Manage the first call to this function ***/
	/* We are supposed to send Hello packet, it shall be already present int the selfPackets(created at channel init) */
	if (event.eventType == BZRTP_EVENT_INIT) {
		if (zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID] == NULL) {
			/* We shall never go through this one because Hello packet shall be created at channel init */
			int retval;
			/* create the Hello packet */
			bzrtpPacket_t *helloPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_HELLO, &retval);
			if (retval != 0) {
				return retval;
			}

			/* build the packet string */
			if (bzrtp_packetBuild(zrtpContext, zrtpChannelContext, helloPacket, zrtpChannelContext->selfSequenceNumber) ==0) {
				zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID] = helloPacket;
			} else {
				bzrtp_freeZrtpPacket(helloPacket);
				return retval;
			}
			/* TODO: Shall add a warning trace here */
		}

		/* it is the first call to this function, so we must also set the timer for retransmissions */
		zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
		zrtpChannelContext->timer.firingTime = 0; /* we must send a first hello message as soon as possible, to do it at first timer tick, we can't do it now because still in initialisation phase and the RTP session may not be ready to send a message */
		zrtpChannelContext->timer.firingCount = 0;
		zrtpChannelContext->timer.timerStep = HELLO_BASE_RETRANSMISSION_STEP;

		zrtpChannelContext->selfSequenceNumber++;
		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* now check the type of packet received, we're expecting either Hello or HelloACK */
		if ((zrtpPacket->messageType != MSGTYPE_HELLO) && (zrtpPacket->messageType != MSGTYPE_HELLOACK)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* parse the packet */
		retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
		if (retval != 0) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return retval;
		}
		/* packet is valid, set the sequence Number in channel context */
		zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

		/* if we have an Hello packet, we must use it to determine which algo we will agree on */
		if (zrtpPacket->messageType == MSGTYPE_HELLO) {
			retval = bzrtp_responseToHelloMessage(zrtpContext, zrtpChannelContext, zrtpPacket);
			if (retval != 0) {
				return retval;
			}

			/* reset the sending Hello timer as peer may have started slowly and lost all our Hello packets */
			zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
			zrtpChannelContext->timer.firingTime = 0;
			zrtpChannelContext->timer.firingCount = 0;
			zrtpChannelContext->timer.timerStep = HELLO_BASE_RETRANSMISSION_STEP;

			/* set next state (do not call it as we will just be waiting for a HelloACK packet from peer, nothing to do) */
			zrtpChannelContext->stateMachine = state_discovery_waitingForHelloAck;
		}

		/* if we have a HelloACK packet, stop the timer and  set next state to state_discovery_waitingForHello */
		if (zrtpPacket->messageType == MSGTYPE_HELLOACK) {
			/* stop the timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* Hello ACK packet is not stored, free it */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* set next state (do not call it as we will just be waiting for a Hello packet from peer, nothing to do) */
			zrtpChannelContext->stateMachine = state_discovery_waitingForHello;

			return 0;
		}


	}

	/*** Manage timer event ***/
	if (event.eventType == BZRTP_EVENT_TIMER) {

		/* adjust timer for next time : check we didn't reach the max retransmissions adjust the step(double it until reaching the cap) */
		if (zrtpChannelContext->timer.firingCount<=HELLO_MAX_RETRANSMISSION_NUMBER) {
			if (2*zrtpChannelContext->timer.timerStep<=HELLO_CAP_RETRANSMISSION_STEP) {
				zrtpChannelContext->timer.timerStep *= 2;
			}
			zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + zrtpChannelContext->timer.timerStep;
		} else { /* we have done enough retransmissions, stop it */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
		}

		/* We must resend a Hello packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
			if (zrtpContext->zrtpCallbacks.bzrtp_sendData!=NULL) {
				zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
				zrtpChannelContext->selfSequenceNumber++;
			}
		} else {
			return retval;
		}

	}

	return 0;
}

/*
 * @brief Arrives in this state coming from init upon reception on Hello ACK, we are now waiting for the Hello packet from peer
 *
 * Arrives from :
 *	- state_discovery_init upon HelloACK reception
 * Goes to:
 * 	- state_keyAgreement_sendingCommit upon Hello reception
 * Send :
 * 	- HelloACK on Hello reception
 *
 */
int state_discovery_waitingForHello(bzrtpEvent_t event)  {
	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	/*** Manage the first call to this function ***/
	/* no init event for this state */

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpEvent_t initEvent;
		int retval;

		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* now check the type of packet received, we're expecting either Hello, HelloACK may arrive but will be discarded as useless now */
		if (zrtpPacket->messageType != MSGTYPE_HELLO) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* parse the packet */
		retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
		if (retval != 0) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return retval;
		}
		/* packet is valid, set the sequence Number in channel context */
		zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

		retval = bzrtp_responseToHelloMessage(zrtpContext, zrtpChannelContext, zrtpPacket);
		if (retval != 0) {
			return retval;
		}

		/* set next state state_keyAgreement_sendingCommit */
		zrtpChannelContext->stateMachine = state_keyAgreement_sendingCommit;

		/* create the init event for next state */
		initEvent.eventType = BZRTP_EVENT_INIT;
		initEvent.bzrtpPacketString = NULL;
		initEvent.bzrtpPacketStringLength = 0;
		initEvent.bzrtpPacket = NULL;
		initEvent.zrtpContext = zrtpContext;
		initEvent.zrtpChannelContext = zrtpChannelContext;

		/* call the next state */
		return zrtpChannelContext->stateMachine(initEvent);
	}

	/*** Manage timer event ***/
	/* no timer event for this state*/
	return 0;
}


/*
 * @brief We are now waiting for the HelloACK packet from peer or a Commit packet
 *
 * Arrives from :
 * 	- state_discovery_init upon Hello reception
 * Goes to:
 * 	- state_keyAgreement_sendingCommit upon HelloACK reception
 * 	- state_keyAgreement_responderSendingDHPart1 upon Commit reception in DHM mode
 * 	- state_confirmation_responderSendingConfirm1 upon Commit reception in non DHM mode
 * Send :
 * 	- Hello until timer's end or transition
 * 	- HelloACK on Hello reception
 *
 */
int state_discovery_waitingForHelloAck(bzrtpEvent_t event) {
	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	int retval;
	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* we can receive either a Hello, HelloACK or Commit packets, others will be ignored */
		if ((zrtpPacket->messageType != MSGTYPE_HELLO) && (zrtpPacket->messageType != MSGTYPE_HELLOACK) && (zrtpPacket->messageType != MSGTYPE_COMMIT)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* We do not need to parse the packet if it is an Hello one as it shall be the duplicate of one we received earlier */
		/* we must check it is the same we initially received, and send a HelloACK */
		if (zrtpPacket->messageType == MSGTYPE_HELLO) {
			bzrtpPacket_t *helloACKPacket;

			if (zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* build and send the HelloACK packet */
			helloACKPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_HELLOACK, &retval);
			if (retval != 0) {
				return retval; /* no need to free the Hello message as it is attached to the context, it will be freed when destroying it */
			}
			retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, helloACKPacket, zrtpChannelContext->selfSequenceNumber);
			if (retval != 0) {
				bzrtp_freeZrtpPacket(helloACKPacket);
				return retval;
			} else {
				/* send the message */
				zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, helloACKPacket->packetString, helloACKPacket->messageLength+ZRTP_PACKET_OVERHEAD);
				zrtpChannelContext->selfSequenceNumber++;
				/* sent HelloACK is not stored, free it */
				bzrtp_freeZrtpPacket(helloACKPacket);
			}

			return 0;
		}

		/* parse the packet wich is either HelloACK or Commit */
		retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
		if (retval != 0) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return retval;
		}

		/* packet is valid, set the sequence Number in channel context */
		zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;


		/* if we have an HelloACK packet, transit to state_keyAgreement_sendingCommit and execute it with an init event */
		if (zrtpPacket->messageType == MSGTYPE_HELLOACK) {
			bzrtpEvent_t initEvent;

			/* stop the timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* set next state to  state_keyAgreement_sendingCommit */
			zrtpChannelContext->stateMachine = state_keyAgreement_sendingCommit;

			/* the HelloACK packet is not stored in context, free it */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* create the init event for next state and call the next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;
			return zrtpChannelContext->stateMachine(initEvent);
		}

		/* if we have a Commit packet we shall turn into responder role
		 * then transit to state_keyAgreement_responderSendingDHPart1 or state_confirmation_responderSendingConfirm1 depending on which mode (Multi/PreShared or DHM) we are using and execute it with an init event */
		if (zrtpPacket->messageType == MSGTYPE_COMMIT) {
			bzrtpCommitMessage_t *commitMessage = (bzrtpCommitMessage_t *)zrtpPacket->messageData;

			/* this will stop the timer, update the context channel and run the next state according to current mode */
			return bzrtp_turnIntoResponder(zrtpContext, zrtpChannelContext, zrtpPacket, commitMessage);
		}


	}

	/*** Manage timer event ***/
	if (event.eventType == BZRTP_EVENT_TIMER) {

		/* adjust timer for next time : check we didn't reach the max retransmissions adjust the step(double it until reaching the cap) */
		if (zrtpChannelContext->timer.firingCount<=HELLO_MAX_RETRANSMISSION_NUMBER) {
			if (2*zrtpChannelContext->timer.timerStep<=HELLO_CAP_RETRANSMISSION_STEP) {
				zrtpChannelContext->timer.timerStep *= 2;
			}
			zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + zrtpChannelContext->timer.timerStep;
		} else { /* we have done enough retransmissions, stop it */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
		}

		/* We must resend a Hello packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		zrtpChannelContext->selfSequenceNumber++;
		} else {
			return retval;
		}

	}

	return 0;
}

/*
 * @brief For any kind of key agreement (DHM, Mult, PreShared), we keep sending commit.
 *
 * Arrives from :
 * 	- state_discovery_waitingForHello upon Hello received
 * 	- state_discovery_waitingForHelloAck upon HelloACK received
 * Goes to:
 * 	- state_keyAgreement_initiatorSendingDHPart2 upon DHPart1 reception in DHM mode
 * 	- state_confirmation_initiatorSendingConfirm2 upon Confirm1 reception in non DHM mode
 * 	- state_keyAgreement_responderSendingDHPart1 upon Commit reception in DHM mode and commit contention gives us the responder role
 * 	- state_confirmation_responderSendingConfirm1 upon Commit reception in non DHM mode and commit contention gives us the responder role
 * Send :
 * 	- Commit until timer's end or transition
 * 	- HelloACK on Hello reception
 *
 */
int state_keyAgreement_sendingCommit(bzrtpEvent_t event) {

	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;
	int retval;

	/*** Manage the first call to this function ***/
	/* We are supposed to send commit packet, check if we have one in the channel Context, the event type shall be INIT in this case */
	if ((event.eventType == BZRTP_EVENT_INIT)  && (zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID] == NULL)) {
		int retval;
		/* create the commit packet */
		bzrtpPacket_t *commitPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_COMMIT, &retval);
		if (retval != 0) {
			return retval;
		}

		/* build the packet string */
		if (bzrtp_packetBuild(zrtpContext, zrtpChannelContext, commitPacket, zrtpChannelContext->selfSequenceNumber) ==0) {
			zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID] = commitPacket;
		} else {
			bzrtp_freeZrtpPacket(commitPacket);
			return retval;
		}

		/* it is the first call to this state function, so we must also set the timer for retransmissions */
		zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
		zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + NON_HELLO_BASE_RETRANSMISSION_STEP;
		zrtpChannelContext->timer.firingCount = 0;
		zrtpChannelContext->timer.timerStep = NON_HELLO_BASE_RETRANSMISSION_STEP;

		/* now send the first Commit message */
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		zrtpChannelContext->selfSequenceNumber++;
		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpEvent_t initEvent;
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* now check the type of packet received, we're expecting a commit or a DHPart1 or a Confirm1 packet */
		if ((zrtpPacket->messageType != MSGTYPE_COMMIT) && (zrtpPacket->messageType != MSGTYPE_DHPART1) && (zrtpPacket->messageType != MSGTYPE_CONFIRM1)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}
		/* DHPART1 can be received only if we are in DHM mode */
		if ((zrtpPacket->messageType == MSGTYPE_DHPART1) && ((zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh))) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}
		/* Confirm1 can be received only if we are in Mult or PreShared mode */
		if ((zrtpPacket->messageType == MSGTYPE_CONFIRM1) && (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Prsh) && (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* if we have a confirm1 and are in multi stream mode, we must first derive s0 and other keys to be able to parse the packet */
		if ((zrtpPacket->messageType == MSGTYPE_CONFIRM1) && (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
			retval = bzrtp_computeS0MultiStreamMode(zrtpContext, zrtpChannelContext);
			if (retval!= 0) {
				return retval;
			}
		}

		/* parse the packet wich is either Commit a DHPart1 or a Confirm1 packet */
		retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
		if (retval != 0) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return retval;
		}

		/* packet is valid, set the sequence Number in channel context */
		zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

		/* create the init event for next state */
		initEvent.eventType = BZRTP_EVENT_INIT;
		initEvent.bzrtpPacketString = NULL;
		initEvent.bzrtpPacketStringLength = 0;
		initEvent.bzrtpPacket = NULL;
		initEvent.zrtpContext = zrtpContext;
		initEvent.zrtpChannelContext = zrtpChannelContext;

		/* we have a DHPart1 - so we are initiator in DHM mode - stop timer and go to state_keyAgreement_initiatorSendingDHPart2 */
		if(zrtpPacket->messageType == MSGTYPE_DHPART1) {
			bzrtpDHPartMessage_t * dhPart1Message;

			/* stop the timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			dhPart1Message = (bzrtpDHPartMessage_t *)zrtpPacket->messageData;

			/* Check shared secret hash found in the DHPart1 message */
			/* if we do not have the secret, don't check it as we do not expect the other part to have it neither */
			/* check matching secret is: check if locally computed(by the initiator) rs1 matches the responder rs1 or rs2 */
			/* if it doesn't check with initiator's rs2 and responder(received in DHPart1) rs1 or rs2 */
			/* In case of cache mismatch, warn the user(reset the Previously Verified Sas Flag) and erase secret as it must not be used to compute s0 */
			if (zrtpContext->cachedSecret.rs1!=NULL) {
				/* check if rs1 match peer's one */
				if (memcmp(zrtpContext->responderCachedSecretHash.rs1ID, dhPart1Message->rs1ID,8) != 0) {
					/* they don't match but self rs1 may match peer rs2 */
					if (memcmp(zrtpContext->responderCachedSecretHash.rs1ID, dhPart1Message->rs2ID,8) != 0) {
						/* They don't match either : erase rs1 and set the cache mismatch flag(which may be unset if self rs2 match peer rs1 or peer rs2 */
						free(zrtpContext->cachedSecret.rs1);
						zrtpContext->cachedSecret.rs1= NULL;
						zrtpContext->cachedSecret.rs1Length = 0;
						zrtpContext->cacheMismatchFlag = 1; /* Do not trigger cache mismatch message for now as it may match rs2 */
					}
				}
			}

			/* we didn't have a match with rs1 and have a rs2; check if it matches responder rs1 or rs2 */
			if ((zrtpContext->cacheMismatchFlag == 1) && (zrtpContext->cachedSecret.rs2!=NULL)) {
				/* does it match rs1 */
				if (memcmp(zrtpContext->responderCachedSecretHash.rs2ID, dhPart1Message->rs1ID,8) != 0) {
					/* it doesn't match rs1 but may match rs2 */
					if (memcmp(zrtpContext->responderCachedSecretHash.rs2ID, dhPart1Message->rs2ID,8) != 0) {
						free(zrtpContext->cachedSecret.rs2);
						zrtpContext->cachedSecret.rs2= NULL;
						zrtpContext->cachedSecret.rs2Length = 0;
					} else { /* it matches rs2, reset the cache mismatch flag */
						zrtpContext->cacheMismatchFlag = 0;
					}
				} else { /* it matches rs1, reset the cache mismatch flag */
					zrtpContext->cacheMismatchFlag = 0;
				}
			}

			/* if we have an aux secret check it match peer's one */
			if (zrtpContext->cachedSecret.auxsecret!=NULL) {
				if (memcmp(zrtpChannelContext->responderAuxsecretID, dhPart1Message->auxsecretID,8) != 0) { // they do not match, set flag to MISMATCH, delete the aux secret as we must not use it
					free(zrtpContext->cachedSecret.auxsecret);
					zrtpContext->cachedSecret.auxsecret= NULL;
					zrtpContext->cachedSecret.auxsecretLength = 0;
					zrtpChannelContext->srtpSecrets.auxSecretMismatch = BZRTP_AUXSECRET_MISMATCH;
				} else { // they do match, set the flag to MATCH  (default is UNSET)
					zrtpChannelContext->srtpSecrets.auxSecretMismatch = BZRTP_AUXSECRET_MATCH;
				}
			}

			if (zrtpContext->cachedSecret.pbxsecret!=NULL) {
				if (memcmp(zrtpContext->responderCachedSecretHash.pbxsecretID, dhPart1Message->pbxsecretID,8) != 0) {
					free(zrtpContext->cachedSecret.pbxsecret);
					zrtpContext->cachedSecret.pbxsecret= NULL;
					zrtpContext->cachedSecret.pbxsecretLength = 0;
				}
			}

			/* in case of cache mismatch, be sure the Previously Verified Sas flag is reset in cache and in the context */
			if (zrtpContext->cacheMismatchFlag == 1) {
				uint8_t pvsFlag = 0;
				const char *colNames[] = {"pvs"};
				uint8_t *colValues[] = {&pvsFlag};
				size_t colLength[] = {1};

				zrtpContext->cachedSecret.previouslyVerifiedSas = 0;
				bzrtp_cache_write_active(zrtpContext, "zrtp", colNames, colValues, colLength, 1);

				/* if we have a statusMessage callback, use it to warn user */
				if (zrtpContext->zrtpCallbacks.bzrtp_statusMessage!=NULL && zrtpContext->zrtpCallbacks.bzrtp_messageLevel>=BZRTP_MESSAGE_ERROR) { /* use error level as this one MUST (RFC section 4.3.2) be warned */
					zrtpContext->zrtpCallbacks.bzrtp_statusMessage(zrtpChannelContext->clientData, BZRTP_MESSAGE_ERROR, BZRTP_MESSAGE_CACHEMISMATCH, NULL);
				}
			}

			/* update context with the information found in the packet */
			memcpy(zrtpChannelContext->peerH[1], dhPart1Message->H1, 32);
			zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID] = zrtpPacket;

			/* Compute the shared DH secret */
			uint16_t pvLength = bzrtp_computeKeyAgreementPublicValueLength(zrtpContext->keyAgreementAlgo, MSGTYPE_DHPART1);
			if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH2k || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH3k) {
				bctbx_DHMContext_t *DHMContext = (bctbx_DHMContext_t *)(zrtpContext->keyAgreementContext);
				DHMContext->peer = (uint8_t *)malloc(pvLength*sizeof(uint8_t));
				memcpy (DHMContext->peer, dhPart1Message->pv, pvLength);
				bctbx_DHMComputeSecret(DHMContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, (void *)zrtpContext->RNGContext);
			}
			if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X255 || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448) {
				bctbx_ECDHContext_t *ECDHContext = (bctbx_ECDHContext_t *)(zrtpContext->keyAgreementContext);
				ECDHContext->peerPublic = (uint8_t *)malloc(pvLength*sizeof(uint8_t));
				memcpy (ECDHContext->peerPublic, dhPart1Message->pv, pvLength);
				bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, (void *)zrtpContext->RNGContext);
			}

			/* Derive the s0 key */
			bzrtp_computeS0DHMMode(zrtpContext, zrtpChannelContext);

			/* set next state to state_keyAgreement_initiatorSendingDHPart2 */
			zrtpChannelContext->stateMachine = state_keyAgreement_initiatorSendingDHPart2;

			/* call it with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}

		/* we have a Confirm1 - so we are initiator and in NON-DHM mode - stop timer and go to state_confirmation_initiatorSendingConfirm2 */
		if(zrtpPacket->messageType == MSGTYPE_CONFIRM1) {
			bzrtpConfirmMessage_t *confirm1Message;

			/* stop the timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* save the message and extract some information from it to the channel context */
			confirm1Message = (bzrtpConfirmMessage_t *)zrtpPacket->messageData;
			memcpy(zrtpChannelContext->peerH[0], confirm1Message->H0, 32);

			/* store the packet to check possible repetitions */
			zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID] = zrtpPacket;

			/* set next state to state_confirmation_responderSendingConfirm2 */
			zrtpChannelContext->stateMachine = state_confirmation_initiatorSendingConfirm2;

			/* call it with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}

		/* we have a commit - do commit contention as in rfc section 4.2 - if we are initiator, keep sending Commits, otherwise stop the timer and go to state_keyAgreement_responderSendingDHPart1 if we are DHM mode or state_confirmation_responderSendingConfirm1 in Multi or PreShared mode */
		if(zrtpPacket->messageType == MSGTYPE_COMMIT) {
			bzrtpCommitMessage_t *peerCommitMessage = (bzrtpCommitMessage_t *)zrtpPacket->messageData;
			bzrtpCommitMessage_t *selfCommitMessage = (bzrtpCommitMessage_t *)zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageData;
			/* - If one Commit is for a DH mode while the other is for Preshared mode, then the Preshared Commit MUST be discarded and the DH Commit proceeds
			 *
			 * - If the two Commits are both Preshared mode, and one party has set the MiTM (M) flag in the Hello message and the other has not, the Commit message from the party who set the (M) flag MUST be discarded, and the one who has not set the (M) flag becomes the initiator, regardless of the nonce values.  In other words, for Preshared mode, the phone is the initiator and the PBX is the responder.
			 *
			 * - If the two Commits are either both DH modes or both non-DH modes, then the Commit message with the lowest hvi (hash value of initiator) value (for DH Commits), or lowest nonce value (for non-DH Commits), MUST be discarded and the other side is the initiator, and the protocol proceeds with the initiator's Commit.  The two hvi or nonce values are compared as large unsigned integers in network byte order.
			 */

			/* we are by default initiator, so just check the statement which turns us into responder */
			if (peerCommitMessage->keyAgreementAlgo != selfCommitMessage->keyAgreementAlgo ) { /* commits have differents modes */
				if ((peerCommitMessage->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Prsh) && (selfCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh)) {
					zrtpChannelContext->role = BZRTP_ROLE_RESPONDER;
				}
			} else { /* commit have the same mode */
				bzrtpHelloMessage_t *peerHelloMessage = (bzrtpHelloMessage_t *)zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData;
				bzrtpHelloMessage_t *selfHelloMessage = (bzrtpHelloMessage_t *)zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageData;

				if (peerCommitMessage->keyAgreementAlgo ==  ZRTP_KEYAGREEMENT_Prsh && ((selfHelloMessage->M == 1) || (peerHelloMessage->M == 1)) ) {
					if (selfHelloMessage->M == 1) { /* we are a PBX -> act as responder */
						zrtpChannelContext->role = BZRTP_ROLE_RESPONDER;
					}
				} else { /* modes are the same and no one has the MiTM flag set : compare hvi/nonce */
					if ((selfCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (selfCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) { /* non DHM mode, compare the nonce, lower will be responder */
						if (memcmp(selfCommitMessage->nonce, peerCommitMessage->nonce, 16) < 0) { /* self nonce < peer nonce */
							zrtpChannelContext->role = BZRTP_ROLE_RESPONDER;
						}

					} else { /* DHM mode, compare the hvi */
						if (memcmp(selfCommitMessage->hvi, peerCommitMessage->hvi, 32) < 0) { /* self hvi < peer hvi */
							zrtpChannelContext->role = BZRTP_ROLE_RESPONDER;
						}
					}
				}
			}

			/* so now check if we are responder - if we are initiator just do nothing, continue sending the commits and ignore the one we just receive */
			if (zrtpChannelContext->role == BZRTP_ROLE_RESPONDER) {
				/* free the self commit packet as it is now useless */
				bzrtp_freeZrtpPacket(zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]);
				zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID] = NULL;
				/* this will update the context channel and run the next state according to current mode */
				return bzrtp_turnIntoResponder(zrtpContext, zrtpChannelContext, zrtpPacket, peerCommitMessage);
			} else { /* we are iniator, just drop the incoming commit packet and keep send our */
				bzrtp_freeZrtpPacket(zrtpPacket);
			}
		}

		return 0;
	}

	/*** Manage timer event ***/
	if (event.eventType == BZRTP_EVENT_TIMER) {

		/* adjust timer for next time : check we didn't reach the max retransmissions adjust the step(double it until reaching the cap) */
		if (zrtpChannelContext->timer.firingCount<=NON_HELLO_MAX_RETRANSMISSION_NUMBER) {
			if (2*zrtpChannelContext->timer.timerStep<=NON_HELLO_CAP_RETRANSMISSION_STEP) {
				zrtpChannelContext->timer.timerStep *= 2;
			}
			zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + zrtpChannelContext->timer.timerStep;
		} else { /* we have done enough retransmissions, stop it */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
		}

		/* We must resend a Commit packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		zrtpChannelContext->selfSequenceNumber++;
		} else {
			return retval;
		}

	}

	return 0;
}


/*
 * @brief For DHM mode only, responder send DHPart1 packet
 *
 * Arrives from:
 *  - state_discovery_waitingForHelloAck upon Commit reception in DHM mode
 *  - state_keyAgreement_sendingCommit upon Commit reception in DHM mode and commit contention gives us the responder role
 * Goes to:
 * 	- state_confirmation_responderSendingConfirm1 upon DHPart2 reception
 * Send :
 * 	- DHPart1 on Commit reception
 *
 */
int state_keyAgreement_responderSendingDHPart1(bzrtpEvent_t event) {

	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;
	int retval;

	/* We are supposed to send DHPart1 packet and it is already in context(built from DHPart when turning into responder) or it's an error */
	if (zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID] == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	/*** Manage the first call to this function ***/
	if (event.eventType == BZRTP_EVENT_INIT) {
		/* There is no timer in this state, make sure it is off */
		zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

		/* now send the first DHPart1 message, note the sequence number has already been incremented when turning the DHPart2 message in DHPart1 */
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* now check the type of packet received, we're expecting DHPart2 or a Commit packet */
		if ((zrtpPacket->messageType != MSGTYPE_COMMIT) && (zrtpPacket->messageType != MSGTYPE_DHPART2)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}


		/* we have a Commit, check it is the same as received previously and resend the DHPart1 packet */
		if(zrtpPacket->messageType == MSGTYPE_COMMIT) {
			if (zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* update and send the DHPart1 packet */
			retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
			if (retval != 0) {
				return retval;
			}

			zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
			zrtpChannelContext->selfSequenceNumber++;

			return 0;
		}

		/* we have a DHPart2 go to state_confirmation_responderSendingConfirm1 */
		if(zrtpPacket->messageType == MSGTYPE_DHPART2) {
			bzrtpDHPartMessage_t * dhPart2Message;
			uint8_t cacheMatchFlag = 0;
			bzrtpEvent_t initEvent;

			/* parse the packet */
			retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
			if (retval != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return retval;
			}

			dhPart2Message = (bzrtpDHPartMessage_t *)zrtpPacket->messageData;

			/* Check shared secret hash found in the DHPart2 message */
			/* if we do not have the secret, don't check it as we do not expect the other part to have it neither */
			/* shared secret matching is: check if initiator rs1(received in DHPart2) match localy computed(by responder from cache) rs1 or rs2 */
			/* of not check if received rs2 match local rs1 or rs2 */
			/* keep the rs1 or rs2 in cachedSecret only if it matches the received one, it will the be used to compute s0 */
			/* In case of cache mismatch, warn the user(reset the previously verified Sas flag) and erase secret as it must not be used to compute s0 */
			if (zrtpContext->cachedSecret.rs1!=NULL) {
				/* check if rs1 match peer's one */
				if (memcmp(zrtpContext->initiatorCachedSecretHash.rs1ID, dhPart2Message->rs1ID,8) != 0) {
					/* they don't match but self rs2 may match peer rs1 */
					if (zrtpContext->cachedSecret.rs2!=NULL) {
						if (memcmp(zrtpContext->initiatorCachedSecretHash.rs2ID, dhPart2Message->rs1ID,8) == 0) {
							/* responder rs2 match initiator rs1, erase responder rs1 */
							cacheMatchFlag = 1;
							free(zrtpContext->cachedSecret.rs1);
							zrtpContext->cachedSecret.rs1= NULL;
							zrtpContext->cachedSecret.rs1Length = 0;
						}
					}
				} else { /* responder rs1 match initiator rs1 */
					cacheMatchFlag = 1;
				}
			}

			/* if we didn't found a match yet(initiator rs1 doesn't match local rs1 or rs2) */
			if ((zrtpContext->cachedSecret.rs1!=NULL) && (cacheMatchFlag == 0)) {
				/* does it match initiator rs2 */
				if (memcmp(zrtpContext->initiatorCachedSecretHash.rs1ID, dhPart2Message->rs2ID,8) != 0) {
					/* it doesn't match rs1 (erase it) but may match rs2 */
					free(zrtpContext->cachedSecret.rs1);
					zrtpContext->cachedSecret.rs1= NULL;
					zrtpContext->cachedSecret.rs1Length = 0;

					if (zrtpContext->cachedSecret.rs2!=NULL) {
						if (memcmp(zrtpContext->initiatorCachedSecretHash.rs2ID, dhPart2Message->rs2ID,8) != 0) {
							/* no match found erase rs2 and set the cache mismatch flag */
							free(zrtpContext->cachedSecret.rs2);
							zrtpContext->cachedSecret.rs2= NULL;
							zrtpContext->cachedSecret.rs2Length = 0;
							zrtpContext->cacheMismatchFlag = 1;
						}
					} else {
							zrtpContext->cacheMismatchFlag = 1;
					}
				}
			}

			/* if we have an auxiliary secret, check it match peer's one */
			if (zrtpContext->cachedSecret.auxsecret!=NULL) {
				if (memcmp(zrtpChannelContext->initiatorAuxsecretID, dhPart2Message->auxsecretID,8) != 0) {  // they do not match, set flag to MISMATCH, delete the aux secret as we must not use it
					free(zrtpContext->cachedSecret.auxsecret);
					zrtpContext->cachedSecret.auxsecret= NULL;
					zrtpContext->cachedSecret.auxsecretLength = 0;
					zrtpChannelContext->srtpSecrets.auxSecretMismatch = BZRTP_AUXSECRET_MISMATCH;
				} else { // they do match, set the flag to MATCH (default is UNSET)
					zrtpChannelContext->srtpSecrets.auxSecretMismatch = BZRTP_AUXSECRET_MATCH;
				}
			}

			if (zrtpContext->cachedSecret.pbxsecret!=NULL) {
				if (memcmp(zrtpContext->initiatorCachedSecretHash.pbxsecretID, dhPart2Message->pbxsecretID,8) != 0) {
					free(zrtpContext->cachedSecret.pbxsecret);
					zrtpContext->cachedSecret.pbxsecret= NULL;
					zrtpContext->cachedSecret.pbxsecretLength = 0;
				}
			}

			/* in case of cache mismatch, be sure the Previously Verified Sas flag is reset in cache and in the context */
			if (zrtpContext->cacheMismatchFlag == 1) {
				uint8_t pvsFlag = 0;
				const char *colNames[] = {"pvs"};
				uint8_t *colValues[] = {&pvsFlag};
				size_t colLength[] = {1};

				zrtpContext->cachedSecret.previouslyVerifiedSas = 0;
				bzrtp_cache_write_active(zrtpContext, "zrtp", colNames, colValues, colLength, 1);

				/* if we have a statusMessage callback, use it to warn user */
				if (zrtpContext->zrtpCallbacks.bzrtp_statusMessage!=NULL && zrtpContext->zrtpCallbacks.bzrtp_messageLevel>=BZRTP_MESSAGE_ERROR) { /* use error level as this one MUST (RFC section 4.3.2) be warned */
					zrtpContext->zrtpCallbacks.bzrtp_statusMessage(zrtpChannelContext->clientData, BZRTP_MESSAGE_ERROR, BZRTP_MESSAGE_CACHEMISMATCH, NULL);
				}
			}


			/* Check that the received PV is not 1 or Prime-1 : is performed by crypto lib */

			/* packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* update context with the information found in the packet */
			memcpy(zrtpChannelContext->peerH[1], dhPart2Message->H1, 32);
			zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID] = zrtpPacket;

			/* Compute the shared DH secret */
			uint16_t pvLength = bzrtp_computeKeyAgreementPublicValueLength(zrtpContext->keyAgreementAlgo, MSGTYPE_DHPART2);
			if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH2k || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH3k) {
				bctbx_DHMContext_t *DHMContext = (bctbx_DHMContext_t *)(zrtpContext->keyAgreementContext);
				DHMContext->peer = (uint8_t *)malloc(pvLength*sizeof(uint8_t));
				memcpy (DHMContext->peer, dhPart2Message->pv, pvLength);
				bctbx_DHMComputeSecret(DHMContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, (void *)zrtpContext->RNGContext);
			}
			if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X255 || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448) {
				bctbx_ECDHContext_t *ECDHContext = (bctbx_ECDHContext_t *)(zrtpContext->keyAgreementContext);
				ECDHContext->peerPublic = (uint8_t *)malloc(pvLength*sizeof(uint8_t));
				memcpy (ECDHContext->peerPublic, dhPart2Message->pv, pvLength);
				bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, (void *)zrtpContext->RNGContext);
			}

			/* Derive the s0 key */
			bzrtp_computeS0DHMMode(zrtpContext, zrtpChannelContext);

			/* create the init event for next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;

			/* set the next state to state_confirmation_responderSendingConfirm1 */
			zrtpChannelContext->stateMachine = state_confirmation_responderSendingConfirm1;

			/* call it with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}

	}

	/*** Manage timer event ***/
	/* no timer for this state, initiator only retransmit packets, we just send DHPart1 when ever a commit arrives */
	return 0;
}

/*
 * @brief For DHM mode only, initiator send DHPart2 packet
 *
 * Arrives from:
 *  - state_keyAgreement_sendingCommit upon DHPart1 reception
 * Goes to:
 * 	- state_confirmation_initiatorSendingConfirm2 upon reception of Confirm1
 * Send :
 * 	- DHPart2 until timer's end or transition
 *
 */
int state_keyAgreement_initiatorSendingDHPart2(bzrtpEvent_t event) {
	int retval;

	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	/*** Manage the first call to this function ***/
	/* We have to send a DHPart2 packet, it is already present in the context */
	if (event.eventType == BZRTP_EVENT_INIT) {
		/* adjust the sequence number and sennd the packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
			zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
			zrtpChannelContext->selfSequenceNumber++;
		} else {
			return retval;
		}

		/* it is the first call to this state function, so we must set the timer for retransmissions */
		zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
		zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + NON_HELLO_BASE_RETRANSMISSION_STEP;
		zrtpChannelContext->timer.firingCount = 0;
		zrtpChannelContext->timer.timerStep = NON_HELLO_BASE_RETRANSMISSION_STEP;

		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* now check the type of packet received, we're expecting a confirm1 packet, DHPart1 packet may arrives, just check they are the same we previously received */
		if ((zrtpPacket->messageType != MSGTYPE_DHPART1) && (zrtpPacket->messageType != MSGTYPE_CONFIRM1)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}



		/* we have DHPart1 packet, just check it is the same we received previously and do nothing */
		if (zrtpPacket->messageType == MSGTYPE_DHPART1) {
			if (zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;


			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			return 0;
		}

		/* we have a confirm1 packet, go to state_confirmation_initiatorSendingConfirm2 state */
		if (zrtpPacket->messageType == MSGTYPE_CONFIRM1) {
			bzrtpConfirmMessage_t *confirm1Packet;
			bzrtpEvent_t initEvent;

			/* parse the packet */
			retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
			if (retval != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return retval;
			}

			/* stop the timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* update context with the information found in the packet */
			confirm1Packet = (bzrtpConfirmMessage_t *)zrtpPacket->messageData;
			memcpy(zrtpChannelContext->peerH[0], confirm1Packet->H0, 32);
			/* on the first channel, set peerPVS in context */
			if (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult) {
				zrtpContext->peerPVS=confirm1Packet->V;
			}

			/* store the packet to check possible repetitions */
			zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID] = zrtpPacket;

			/* packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* create the init event for next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;

			/* next state is state_confirmation_initiatorSendingConfirm2*/
			zrtpChannelContext->stateMachine = state_confirmation_initiatorSendingConfirm2;

			/* call the next state with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}
	}


	/*** Manage timer event ***/
	if (event.eventType == BZRTP_EVENT_TIMER) {

		/* adjust timer for next time : check we didn't reach the max retransmissions adjust the step(double it until reaching the cap) */
		if (zrtpChannelContext->timer.firingCount<=NON_HELLO_MAX_RETRANSMISSION_NUMBER) {
			if (2*zrtpChannelContext->timer.timerStep<=NON_HELLO_CAP_RETRANSMISSION_STEP) {
				zrtpChannelContext->timer.timerStep *= 2;
			}
			zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + zrtpChannelContext->timer.timerStep;
		} else { /* we have done enough retransmissions, stop it */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
		}

		/* We must resend a DHPart1 packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
			zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
			zrtpChannelContext->selfSequenceNumber++;
		} else {
			return retval;
		}

	}

	return 0;
}


/*
 * @brief Responder send the confirm1 message
 *
 * Arrives from:
 * - state_keyAgreement_responderSendingDHPart1 upon DHPart2 reception
 * - state_keyAgreement_sendingCommit upon Commit reception in non DHM mode and commit contention gives us the responder role
 * - state_discovery_waitingForHelloAck upon Commit reception in non DHM mode
 * Goes to:
 * - state_secure on Confirm2 reception
 * Send :
 * - Confirm1 on Commit or DHPart2 reception
 *
 */
int state_confirmation_responderSendingConfirm1(bzrtpEvent_t event) {
	int retval;

	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	/*** Manage the first call to this function ***/
	if (event.eventType == BZRTP_EVENT_INIT) {
		bzrtpPacket_t *confirm1Packet;

		/* when in multistream mode, we must derive s0 and other keys from ZRTPSess */
		if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult) {
			/* we need ZRTPSess */
			if (zrtpContext->ZRTPSess == NULL) {
				return BZRTP_ERROR_INVALIDCONTEXT;
			}
			retval = bzrtp_computeS0MultiStreamMode(zrtpContext, zrtpChannelContext);
			if (retval!= 0) {
				return retval;
			}
		} else if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) { /* when in preShared mode: todo */
		} else { /* we are in DHM mode, check that we have the keys needed to build the confirm1 packet */
			/* we must build the confirm1 packet, check in the channel context if we have the needed keys */
			if ((zrtpChannelContext->mackeyr == NULL) || (zrtpChannelContext->zrtpkeyr == NULL)) {
				return BZRTP_ERROR_INVALIDCONTEXT;
			}
		}

		/* There is no timer in this state, make sure it is off */
		zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

		/* build the confirm1 packet */
		confirm1Packet = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_CONFIRM1, &retval);
		if (retval!=0) {
			return retval;
		}
		retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, confirm1Packet, zrtpChannelContext->selfSequenceNumber);
		if (retval!=0) {
			bzrtp_freeZrtpPacket(confirm1Packet);
			return retval;
		}
		zrtpChannelContext->selfSequenceNumber++;

		/* save it so we can send it again if needed */
		zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID] = confirm1Packet;

		/* now send the confirm1 message */
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* we expect packets Confirm2, Commit in NON-DHM mode  or DHPart2 in DHM mode */
		if ((zrtpPacket->messageType != MSGTYPE_CONFIRM2) && (zrtpPacket->messageType != MSGTYPE_COMMIT) && (zrtpPacket->messageType != MSGTYPE_DHPART2)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* We have a commit packet, check we are in non DHM mode, that the commit is identical to the one we already had and resend the Confirm1 packet */
		if (zrtpPacket->messageType == MSGTYPE_COMMIT) {
			/* Check we are not in DHM mode */
			if ((zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Prsh) && (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult)) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
			}

			/* Check the commit packet is the same we already had */
			if (zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* update sequence number and resend confirm1 packet */
			retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
			if (retval!=0) {
				return retval;
			}
			zrtpChannelContext->selfSequenceNumber++;
			return zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		}

		/* We have a DHPart2 packet, check we are in DHM mode, that the DHPart2 is identical to the one we already had and resend the Confirm1 packet */
		if (zrtpPacket->messageType == MSGTYPE_DHPART2) {
			/* Check we are not DHM mode */
			if ((zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
			}

			/* Check the DHPart2 packet is the same we already had */
			if (zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* update sequence number and resend confirm1 packet */
			retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
			if (retval!=0) {
				return retval;
			}
			zrtpChannelContext->selfSequenceNumber++;
			return zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		}

		/* We have a Confirm2 packet, check it, send a conf2ACK and go to secure state */
		if (zrtpPacket->messageType == MSGTYPE_CONFIRM2) {
			bzrtpConfirmMessage_t *confirm2Packet;
			bzrtpPacket_t *conf2ACKPacket;
			bzrtpEvent_t initEvent;

			/* parse the packet */
			retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
			if (retval != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return retval;
			}

			/* update context with the information found in the packet */
			confirm2Packet = (bzrtpConfirmMessage_t *)zrtpPacket->messageData;
			memcpy(zrtpChannelContext->peerH[0], confirm2Packet->H0, 32);
			/* on the first channel, set peerPVS in context */
			if (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult) {
				zrtpContext->peerPVS = confirm2Packet->V;
			}

			/* store the packet to check possible repetitions : note the storage points to confirm1, delete it as we don't need it anymore */
			bzrtp_freeZrtpPacket(zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]);
			zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID] = zrtpPacket;

			/* packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* compute SAS and srtp secrets */
			retval = bzrtp_deriveSrtpKeysFromS0(zrtpContext, zrtpChannelContext);
			if (retval!=0) {
				return retval;
			}

			/* compute and update in cache the retained shared secret */
			bzrtp_updateCachedSecrets(zrtpContext, zrtpChannelContext);

			/* send them to the environment for receiver as we may receive a srtp packet in response to our conf2ACK */
			if (zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable != NULL) {
				zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable(zrtpChannelContext->clientData, &zrtpChannelContext->srtpSecrets, ZRTP_SRTP_SECRETS_FOR_RECEIVER);
			}

			/* create and send a conf2ACK packet */
			conf2ACKPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_CONF2ACK, &retval);
			if (retval!=0) {
				return retval;
			}
			retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, conf2ACKPacket, zrtpChannelContext->selfSequenceNumber);
			if (retval!=0) {
				bzrtp_freeZrtpPacket(conf2ACKPacket);
				return retval;
			}
			zrtpChannelContext->selfSequenceNumber++;

			retval = zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, conf2ACKPacket->packetString, conf2ACKPacket->messageLength+ZRTP_PACKET_OVERHEAD);
			bzrtp_freeZrtpPacket(conf2ACKPacket);
			if (retval!=0) {
				return retval;
			}

			/* send SRTP secrets for sender as we can start sending srtp packets just after sending the conf2ACK */
			if (zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable != NULL) {
				zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable(zrtpChannelContext->clientData, &zrtpChannelContext->srtpSecrets, ZRTP_SRTP_SECRETS_FOR_SENDER);
			}

			/* create the init event for next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;

			/* next state is state_secure */
			zrtpChannelContext->stateMachine = state_secure;

			/* call the next state with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}
	}

	/*** Manage timer event ***/
	/* no timer for this state, initiator only retransmit packets, we just send confirm11 when ever a DHPart2 or commit arrives */

	return 0;
}


/*
 * @brief Initiator send the confirm2 message
 *
 * Arrives from:
 * - state_keyAgreement_initiatorSendingDHPart2 upon confirm1 reception
 * - state_keyAgreement_sendingCommit upon Confirm1 reception in non DHM mode
 * Goes to:
 * - state_secure on Conf2ACK reception or first SRTP message
 * Send :
 * - Confirm2 until timer's end or transition
 *
 */
int state_confirmation_initiatorSendingConfirm2(bzrtpEvent_t event) {
	int retval;

	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	/*** Manage the first call to this function ***/
	if (event.eventType == BZRTP_EVENT_INIT) {
		bzrtpPacket_t *confirm2Packet;

		/* we must build the confirm2 packet, check in the channel context if we have the needed keys */
		if ((zrtpChannelContext->mackeyi == NULL) || (zrtpChannelContext->zrtpkeyi == NULL)) {
			return BZRTP_ERROR_INVALIDCONTEXT;
		}

		/* build the confirm2 packet */
		confirm2Packet = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_CONFIRM2, &retval);
		if (retval!=0) {
			return retval;
		}
		retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, confirm2Packet, zrtpChannelContext->selfSequenceNumber);
		if (retval!=0) {
			bzrtp_freeZrtpPacket(confirm2Packet);
			return retval;
		}

		/* save it so we can send it again if needed */
		zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID] = confirm2Packet;

		/* compute SAS and SRTP secrets as responder may directly send SRTP packets and non conf2ACK */
		retval = bzrtp_deriveSrtpKeysFromS0(zrtpContext, zrtpChannelContext);
		if (retval!=0) {
			return retval;
		}

		/* send them to the environment, but specify they are for receiver only as we must wait for the conf2ACK or the first valid srtp packet from peer
		 * to start sending srtp packets */
		if (zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable != NULL) {
			zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable(zrtpChannelContext->clientData, &zrtpChannelContext->srtpSecrets, ZRTP_SRTP_SECRETS_FOR_RECEIVER);
		}

		/* now send the confirm2 message */
		retval = zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
		if (retval != 0) {
			return retval;
		}
		zrtpChannelContext->selfSequenceNumber++;


		/* it is the first call to this state function, so we must set the timer for retransmissions */
		zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
		zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + NON_HELLO_BASE_RETRANSMISSION_STEP;
		zrtpChannelContext->timer.firingCount = 0;
		zrtpChannelContext->timer.timerStep = NON_HELLO_BASE_RETRANSMISSION_STEP;

	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* we expect packets Conf2ACK, or a Confirm1 */
		if ((zrtpPacket->messageType != MSGTYPE_CONFIRM1) && (zrtpPacket->messageType != MSGTYPE_CONF2ACK)) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* we have confirm1 packet, just check it is the same we received previously and do nothing */
		if (zrtpPacket->messageType == MSGTYPE_CONFIRM1) {
			if (zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}
			if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength) != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
			}

			/* incoming packet is valid, set the sequence Number in channel context */
			zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			return 0;
		}

		/* we have a conf2ACK packet, parse it to validate it, stop the timer and go to secure state */
		if (zrtpPacket->messageType == MSGTYPE_CONF2ACK) {
			bzrtpEvent_t initEvent;

			/* parse the packet */
			retval = bzrtp_packetParser(zrtpContext, zrtpChannelContext, event.bzrtpPacketString, event.bzrtpPacketStringLength, zrtpPacket);
			if (retval != 0) {
				bzrtp_freeZrtpPacket(zrtpPacket);
				return retval;
			}

			/* free the incoming packet */
			bzrtp_freeZrtpPacket(zrtpPacket);

			/* stop the retransmission timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* compute and update in cache the retained shared secret */
			bzrtp_updateCachedSecrets(zrtpContext, zrtpChannelContext);

			/* send the sender srtp keys to the client(we sent receiver only when the first confirm1 packet arrived) */
			if (zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable != NULL) {
				zrtpContext->zrtpCallbacks.bzrtp_srtpSecretsAvailable(zrtpChannelContext->clientData, &zrtpChannelContext->srtpSecrets, ZRTP_SRTP_SECRETS_FOR_SENDER);
			}

			/* create the init event for next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;

			/* next state is state_secure */
			zrtpChannelContext->stateMachine = state_secure;

			/* call the next state with the init event */
			return zrtpChannelContext->stateMachine(initEvent);
		}
	}

	/*** Manage timer event ***/
	if (event.eventType == BZRTP_EVENT_TIMER) {

		/* adjust timer for next time : check we didn't reach the max retransmissions adjust the step(double it until reaching the cap) */
		if (zrtpChannelContext->timer.firingCount<=NON_HELLO_MAX_RETRANSMISSION_NUMBER) {
			if (2*zrtpChannelContext->timer.timerStep<=NON_HELLO_CAP_RETRANSMISSION_STEP) {
				zrtpChannelContext->timer.timerStep *= 2;
			}
			zrtpChannelContext->timer.firingTime = zrtpContext->timeReference + zrtpChannelContext->timer.timerStep;
		} else { /* we have done enough retransmissions, stop it */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
		}

		/* We must resend a Confirm2 packet */
		retval = bzrtp_packetUpdateSequenceNumber(zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
		if (retval == 0) {
			zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->packetString, zrtpChannelContext->selfPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength+ZRTP_PACKET_OVERHEAD);
			zrtpChannelContext->selfSequenceNumber++;
		} else {
			return retval;
		}

	}

	return 0;
}

/*
 * @brief We are in secure state
 *
 * Arrives from:
 * 	- state_confirmation_responderSendingConfirm1 on Confirm2 reception
 * 	- state_confirmation_initiatorSendingConfirm2 on conf2ACK or first SRTP message
 * Goes to:
 * 	- This is the end(we do not support GoClear message), state machine may be destroyed after going to secure mode
 * Send :
 * 	- Conf2ACK on Confirm2 reception
 *
 */
int state_secure(bzrtpEvent_t event) {
	/* get the contextes from the event */
	bzrtpContext_t *zrtpContext = event.zrtpContext;
	bzrtpChannelContext_t *zrtpChannelContext = event.zrtpChannelContext;

	/*** Manage the first call to this function ***/
	if (event.eventType == BZRTP_EVENT_INIT) {
		/* there is no timer in this state, turn it off */
		zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

		/* if we are not in Multistream mode, turn the global context isSecure flag to 1 */
		if (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult) {
			zrtpContext->isSecure = 1;
		}
		/* turn channel isSecure flag to 1 */
		zrtpChannelContext->isSecure = 1;

		/* call the environment to signal we're ready to operate */
		if (zrtpContext->zrtpCallbacks.bzrtp_startSrtpSession!= NULL) {
			zrtpContext->zrtpCallbacks.bzrtp_startSrtpSession(zrtpChannelContext->clientData, &(zrtpChannelContext->srtpSecrets), zrtpContext->cachedSecret.previouslyVerifiedSas && zrtpContext->peerPVS);
		}
		return 0;
	}

	/*** Manage message event ***/
	if (event.eventType == BZRTP_EVENT_MESSAGE) {
		int retval;
		bzrtpPacket_t *conf2ACKPacket;
		bzrtpPacket_t *zrtpPacket = event.bzrtpPacket;

		/* we expect confirm2 packet */
		if (zrtpPacket->messageType != MSGTYPE_CONFIRM2) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
		}

		/* We have a confirm2 packet, check it is identical to the one we already had and resend the Conf2ACK packet */
		/* Check the commit packet is the same we already had */
		if (zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength != zrtpPacket->messageLength) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
		}
		if (memcmp(event.bzrtpPacketString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[CONFIRM_MESSAGE_STORE_ID]->messageLength) != 0) {
			bzrtp_freeZrtpPacket(zrtpPacket);
			return BZRTP_ERROR_UNMATCHINGPACKETREPETITION;
		}

		/* incoming packet is valid, set the sequence Number in channel context */
		zrtpChannelContext->peerSequenceNumber = zrtpPacket->sequenceNumber;

		/* free the incoming packet */
		bzrtp_freeZrtpPacket(zrtpPacket);

		/* create and send a conf2ACK packet */
		conf2ACKPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_CONF2ACK, &retval);
		if (retval!=0) {
			return retval;
		}
		retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, conf2ACKPacket, zrtpChannelContext->selfSequenceNumber);
		if (retval!=0) {
			bzrtp_freeZrtpPacket(conf2ACKPacket);
			return retval;
		}
		zrtpChannelContext->selfSequenceNumber++;

		retval = zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, conf2ACKPacket->packetString, conf2ACKPacket->messageLength+ZRTP_PACKET_OVERHEAD);
		/* free the conf2ACK packet */
		bzrtp_freeZrtpPacket(conf2ACKPacket);

		return retval;
	}


	/*** Manage timer event ***/
	/* no timer in this state */

	return 0;
}

/**
 * @brief Turn the current Channel into responder role
 * This happens when receiving a commit message when in state state_discovery_waitingForHelloAck or state_keyAgreement_sendingCommit if commit contention gives us the responder role.
 * State will be changed to state_confirmation_responderSendingConfirm1 or state_confirmation_responderSendingDHPart1 depending on DHM or non-DHM operation mode
 *
 * @param[in]		zrtpContext				The current zrtp Context
 * @param[in,out]	zrtpChannelContext		The channel we are operating
 * @param[in]		zrtpPacket				The zrtpPacket receives, it contains the commit message
 * @param[in]		commitMessage			A direct pointer to the commitMessage structure contained in the zrtp packet
 *
 * @return 0 on succes, error code otherwise
 */
int bzrtp_turnIntoResponder(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket, bzrtpCommitMessage_t *commitMessage) {
			bzrtpEvent_t initEvent;

			/* kill the ongoing timer */
			zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

			/* store the commit packet in the channel context as it is needed later to check MAC */
			zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID] = zrtpPacket;

			/* save the peer H2 */
			memcpy(zrtpChannelContext->peerH[2], commitMessage->H2, 32); /* H2 */

			/* we are receiver, set it in the context and update our selected algos */
			zrtpChannelContext->role = BZRTP_ROLE_RESPONDER;
			zrtpChannelContext->hashAlgo = commitMessage->hashAlgo;
			zrtpChannelContext->cipherAlgo = commitMessage->cipherAlgo;
			zrtpChannelContext->authTagAlgo = commitMessage->authTagAlgo;
			zrtpChannelContext->keyAgreementAlgo = commitMessage->keyAgreementAlgo;
			zrtpChannelContext->sasAlgo = commitMessage->sasAlgo;
			bzrtp_updateCryptoFunctionPointers(zrtpChannelContext);

			/* if we have a self DHPart packet (means we are in DHM mode) we must rebuild the self DHPart packet to be responder and not initiator */
			/* as responder we must swap the aux shared secret between responder and initiator as they are computed using the H3 and not a constant string */
			if (zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID] != NULL) {
				uint8_t tmpBuffer[8];
				bzrtpDHPartMessage_t *selfDHPart1Packet;
				int retval;

				memcpy(tmpBuffer, zrtpChannelContext->initiatorAuxsecretID, 8);
				memcpy(zrtpChannelContext->initiatorAuxsecretID, zrtpChannelContext->responderAuxsecretID, 8);
				memcpy(zrtpChannelContext->responderAuxsecretID, tmpBuffer, 8);

				/* responder self DHPart packet is DHPart1, change the type (we created a DHPart2) */
				zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageType = MSGTYPE_DHPART1;

				/* change the shared secret ID to the responder one (we set them by default to the initiator's one) */
				selfDHPart1Packet = (bzrtpDHPartMessage_t *)zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageData;
				memcpy(selfDHPart1Packet->rs1ID, zrtpContext->responderCachedSecretHash.rs1ID, 8);
				memcpy(selfDHPart1Packet->rs2ID, zrtpContext->responderCachedSecretHash.rs2ID, 8);
				memcpy(selfDHPart1Packet->auxsecretID, zrtpChannelContext->responderAuxsecretID, 8);
				memcpy(selfDHPart1Packet->pbxsecretID, zrtpContext->responderCachedSecretHash.pbxsecretID, 8);

				/* free the packet string and rebuild the packet */
				free(zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString);
				zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString = NULL;
				retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID], zrtpChannelContext->selfSequenceNumber);
				if (retval == 0) {
					zrtpChannelContext->selfSequenceNumber++;
				} else {
					return retval;
				}
			}

			/* create the init event for next state */
			initEvent.eventType = BZRTP_EVENT_INIT;
			initEvent.bzrtpPacketString = NULL;
			initEvent.bzrtpPacketStringLength = 0;
			initEvent.bzrtpPacket = NULL;
			initEvent.zrtpContext = zrtpContext;
			initEvent.zrtpChannelContext = zrtpChannelContext;

			/* if we are in PreShared or Multi mode, go to state_confirmation_responderSendingConfirm1 */
			if ((zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
				/* set next state to state_confirmation_responderSendingConfirm1 */
				zrtpChannelContext->stateMachine = state_confirmation_responderSendingConfirm1;

				/* call it with the init event */
				return zrtpChannelContext->stateMachine(initEvent);

			} else {/* if we are in DHM mode, goes to state_keyAgreement_responderSendingDHPart1 */
				/* set next state to state_keyAgreement_responderSendingDHPart1 */
				zrtpChannelContext->stateMachine = state_keyAgreement_responderSendingDHPart1;

				/* call it with the init event */
				return zrtpChannelContext->stateMachine(initEvent);
			}
}

/**
 * @brief When a Hello message arrive from peer for the first time, we shall parse it to check if it match our configuration and act on the context
 * This message may arrives when in state state_discovery_init or state_discovery_waitingForHello.
 * - Find agreement on algo to use
 * - Check if we have retained secrets in cache matching the peer ZID
 * - if agreed on a DHM mode : compute the public value and prepare a DHPart2 packet(assume we are initiator, change later if needed)
 * - if agreed on a non-DHM mode : compute s0 and derive keys from it TODO
 *
 * @param[in]		zrtpContext				The current zrtp Context
 * @param[in,out]	zrtpChannelContext		The channel we are operating
 * @param[in]		zrtpPacket				The zrtpPacket received, it contains the hello message
 *
 * @return 0 on succes, error code otherwise
 */
int bzrtp_responseToHelloMessage(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket) {
	int retval;
	int i;
	uint8_t peerSupportMultiChannel = 0;
	bzrtpPacket_t *helloACKPacket;
	bzrtpHelloMessage_t *helloMessage = (bzrtpHelloMessage_t *)zrtpPacket->messageData;

	/* check supported version of ZRTP protocol */
	if (memcmp(helloMessage->version, ZRTP_VERSION, 3) != 0) { /* we support version 1.10 only but checking is done on 1.1? as explained in rfc section 4.1.1 */
		bzrtp_freeZrtpPacket(zrtpPacket);
		return BZRTP_ERROR_UNSUPPORTEDZRTPVERSION; /* packet shall be ignored*/
	}

	/* now check we have some algo in common. zrtpChannelContext will be updated with common algos if found */
	retval = bzrtp_cryptoAlgoAgreement(zrtpContext, zrtpChannelContext, helloMessage);
	if(retval != 0) {
		bzrtp_freeZrtpPacket(zrtpPacket);
		return retval;
	}

	/* check if the peer accept MultiChannel */
	for (i=0; i<helloMessage->kc; i++) {
		if (helloMessage->supportedKeyAgreement[i] == ZRTP_KEYAGREEMENT_Mult) {
			peerSupportMultiChannel = 1;
		}
	}
	zrtpContext->peerSupportMultiChannel = peerSupportMultiChannel;

	/* copy into the channel context the relevant informations */
	memcpy(zrtpContext->peerZID, helloMessage->ZID, 12); /* peer ZID */
	memcpy(zrtpChannelContext->peerH[3], helloMessage->H3, 32); /* H3 */
	zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID] = zrtpPacket; /* peer hello packet */

	/* Extract from peerHello packet the version of bzrtp used by peer and store it in global context(needed later when exporting keys)
	 * Bzrtp version started to be publicised in version 1.1, before that the client identifier was simply BZRTP and earlier version also have LINPHONE-ZRTPCPP
	 * We must know this version in order to keep export key computation compatible between version as before v1.1 it was badly implemented
	 * So basically check :
	 *	if the client identifier is BZRTP or LINPHONE-ZRTPCPP -> version 1.0 (use the old and incorrect way of creating an export key)
	 * 	if BZRTPv1.1 -> version 1.1 (use RFC compliant way of creating the export key )
	 * 	if anything else peer use another library, set version to 0.0 and use RFC compliant way of creating the export key as peer library is expected to be RFC compliant
	*/
	/* This is BZRTP in its old version */
	if ((strncmp(ZRTP_CLIENT_IDENTIFIERv1_0a, (char *)helloMessage->clientIdentifier, 16)==0) || (strncmp(ZRTP_CLIENT_IDENTIFIERv1_0b, (char *)helloMessage->clientIdentifier, 16)==0)){
		zrtpContext->peerBzrtpVersion=0x010000;
		if (zrtpContext->zrtpCallbacks.bzrtp_statusMessage!=NULL && zrtpContext->zrtpCallbacks.bzrtp_messageLevel>=BZRTP_MESSAGE_WARNING) { /* use warning level as the client may really wants to know this */
				zrtpContext->zrtpCallbacks.bzrtp_statusMessage(zrtpChannelContext->clientData, BZRTP_MESSAGE_WARNING, BZRTP_MESSAGE_PEERVERSIONOBSOLETE, (const char *)helloMessage->clientIdentifier);
		}
	} else if (strncmp(ZRTP_CLIENT_IDENTIFIERv1_1, (char *)helloMessage->clientIdentifier, 16)==0) { /* peer has the current version, everything is Ok */
		zrtpContext->peerBzrtpVersion=0x010100;
	} else { /* peer uses another lib, we're probably not LIME compliant, log it */
		zrtpContext->peerBzrtpVersion=0;
		if (zrtpContext->zrtpCallbacks.bzrtp_statusMessage!=NULL && zrtpContext->zrtpCallbacks.bzrtp_messageLevel>=BZRTP_MESSAGE_LOG) {
				zrtpContext->zrtpCallbacks.bzrtp_statusMessage(zrtpChannelContext->clientData, BZRTP_MESSAGE_LOG, BZRTP_MESSAGE_PEERNOTBZRTP, (const char *)helloMessage->clientIdentifier);
		}
	}

	/* now select mode according to context */
	if ((zrtpContext->peerSupportMultiChannel) == 1 && (zrtpContext->ZRTPSess != NULL)) { /* if we support multichannel and already have a ZRTPSess key, switch to multichannel mode */
		zrtpChannelContext->keyAgreementAlgo = ZRTP_KEYAGREEMENT_Mult;
	} else { /* we are not in multiStream mode, so we shall compute the hash of shared secrets */
		/* get from cache, if relevant, the retained secrets associated to the peer ZID */
		if (zrtpContext->cachedSecret.rs1 == NULL) { /* if we do not have already secret hashes in this session context. Note, they may be updated in cache file but they also will be in the context at the same time, so no need to parse the cache again */
			bzrtp_getPeerAssociatedSecrets(zrtpContext, helloMessage->ZID);
		}

		/* now compute the retained secret hashes (secrets may be updated but not their hash) as in rfc section 4.3.1 */
		if (zrtpContext->cachedSecret.rs1!=NULL) {
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.rs1, zrtpContext->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, zrtpContext->initiatorCachedSecretHash.rs1ID);
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.rs1, zrtpContext->cachedSecret.rs1Length, (uint8_t *)"Responder", 9, 8, zrtpContext->responderCachedSecretHash.rs1ID);
		} else { /* we have no secret, generate a random */
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->initiatorCachedSecretHash.rs1ID, 8);
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->responderCachedSecretHash.rs1ID, 8);
		}

		if (zrtpContext->cachedSecret.rs2!=NULL) {
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.rs2, zrtpContext->cachedSecret.rs2Length, (uint8_t *)"Initiator", 9, 8, zrtpContext->initiatorCachedSecretHash.rs2ID);
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.rs2, zrtpContext->cachedSecret.rs2Length, (uint8_t *)"Responder", 9, 8, zrtpContext->responderCachedSecretHash.rs2ID);
		} else { /* we have no secret, generate a random */
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->initiatorCachedSecretHash.rs2ID, 8);
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->responderCachedSecretHash.rs2ID, 8);
		}


		if (zrtpContext->cachedSecret.pbxsecret!=NULL) {
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.pbxsecret, zrtpContext->cachedSecret.pbxsecretLength, (uint8_t *)"Initiator", 9, 8, zrtpContext->initiatorCachedSecretHash.pbxsecretID);
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.pbxsecret, zrtpContext->cachedSecret.pbxsecretLength, (uint8_t *)"Responder", 9, 8, zrtpContext->responderCachedSecretHash.pbxsecretID);
		} else { /* we have no secret, generate a random */
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->initiatorCachedSecretHash.pbxsecretID, 8);
			bctbx_rng_get(zrtpContext->RNGContext, zrtpContext->responderCachedSecretHash.pbxsecretID, 8);
		}

		/* if we have any transient auxiliary secret, append it to the one found in cache */
		if (zrtpContext->transientAuxSecret!=NULL) {
			zrtpContext->cachedSecret.auxsecret = (uint8_t *)realloc(zrtpContext->cachedSecret.auxsecret, zrtpContext->cachedSecret.auxsecretLength + zrtpContext->transientAuxSecretLength);
			memcpy(zrtpContext->cachedSecret.auxsecret + zrtpContext->cachedSecret.auxsecretLength, zrtpContext->transientAuxSecret, zrtpContext->transientAuxSecretLength);
			zrtpContext->cachedSecret.auxsecretLength += zrtpContext->transientAuxSecretLength;
		}

		if (zrtpContext->cachedSecret.auxsecret!=NULL) {
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.auxsecret, zrtpContext->cachedSecret.auxsecretLength, zrtpChannelContext->selfH[3], 32, 8, zrtpChannelContext->initiatorAuxsecretID);
			zrtpChannelContext->hmacFunction(zrtpContext->cachedSecret.auxsecret, zrtpContext->cachedSecret.auxsecretLength, zrtpChannelContext->peerH[3], 32, 8, zrtpChannelContext->responderAuxsecretID);
		} else { /* we have no secret, generate a random */
			bctbx_rng_get(zrtpContext->RNGContext, zrtpChannelContext->initiatorAuxsecretID, 8);
			bctbx_rng_get(zrtpContext->RNGContext, zrtpChannelContext->responderAuxsecretID, 8);
		}

	}

	/* When in PreShared mode Derive ZRTPSess, s0 from the retained secret and then all the other keys */
	if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) {
		/*TODO*/
	} else if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult) { /* when in Multistream mode, do nothing, will derive s0 from ZRTPSess when we know who is initiator */

	} else { /* when in DHM mode : Create the DHPart2 packet (that we then may change to DHPart1 if we ended to be the responder)*/
		bzrtpPacket_t *selfDHPartPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_DHPART2, &retval);
		if (retval != 0) {
			return retval; /* no need to free the Hello message as it is attached to the context, it will be freed when destroying it */
		}

		retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, selfDHPartPacket, 0); /* we don't care now about sequence number as we just need to build the message to be able to insert a hash of it into the commit packet */
		if (retval == 0) { /* ok, insert it in context as we need it to build the commit packet */
			zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID] = selfDHPartPacket;
		} else {
			return retval; /* no need to free the Hello message as it is attached to the context, it will be freed when destroying it */
		}
	}

	/* now respond to this Hello packet sending a Hello ACK */
	helloACKPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_HELLOACK, &retval);
	if (retval != 0) {
		return retval; /* no need to free the Hello message as it is attached to the context, it will be freed when destroying it */
	}
	retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, helloACKPacket, zrtpChannelContext->selfSequenceNumber);
	if (retval != 0) {
		bzrtp_freeZrtpPacket(helloACKPacket);
		return retval;
	} else {
		/* send the message */
		zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, helloACKPacket->packetString, helloACKPacket->messageLength+ZRTP_PACKET_OVERHEAD);
		zrtpChannelContext->selfSequenceNumber++;
		bzrtp_freeZrtpPacket(helloACKPacket);
	}
	return 0;
}

/**
 * @brief After the DHPart1 or DHPart2 arrives from peer, validity check and shared secret computation
 * call this function to compute s0, KDF Context, ZRTPSess,
 *
 * param[in]	zrtpContext			The context we are operation on(where to find the DHM context with the shared secret ready)
 * param[in]	zrtpChannelContext	The channel context we are operation on
 *
 * return 0 on success, error code otherwise
 */
int bzrtp_computeS0DHMMode(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {

	uint8_t *dataToHash; /* a buffer used to store concatened data to be hashed */
	uint16_t hashDataLength; /* Length of the buffer */
	uint16_t hashDataIndex; /* an index used while filling the buffer */

	uint8_t *ZIDi; /* a pointer to the 12 bytes string initiator's ZID */
	uint8_t *ZIDr; /* a pointer to the 12 bytes string responder's ZID */

	uint8_t *totalHash;

	uint8_t *s1=NULL; /* s1 is rs1 if we have it, rs2 otherwise, or null if we do not have rs2 too */
	uint32_t s1Length=0;
	uint8_t *s2=NULL; /* s2 is aux secret if we have it, null otherwise */
	uint32_t s2Length=0;
	uint8_t *s3=NULL; /* s3 is pbx secret if we have it, null otherwise */
	uint32_t s3Length=0;

	/* first compute the total_hash as in rfc section 4.4.1.4
	 * total_hash = hash(Hello of responder || Commit || DHPart1 || DHPart2)
	 * total_hash length depends on the agreed hash algo
	 */
	if (zrtpChannelContext->role == BZRTP_ROLE_RESPONDER) {
		hashDataLength = zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength;

		dataToHash = (uint8_t *)malloc(hashDataLength*sizeof(uint8_t));
		hashDataIndex = 0;

		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength);

		ZIDi = zrtpContext->peerZID;
		ZIDr = zrtpContext->selfZID;

	} else { /* we are initiator */

		hashDataLength = zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength
			+ zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength;

		dataToHash = (uint8_t *)malloc(hashDataLength*sizeof(uint8_t));
		hashDataIndex = 0;

		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex +=zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength);

		ZIDi = zrtpContext->selfZID;
		ZIDr = zrtpContext->peerZID;
	}

	totalHash = (uint8_t *)malloc(zrtpChannelContext->hashLength);
	zrtpChannelContext->hashFunction(dataToHash, hashDataLength, zrtpChannelContext->hashLength, totalHash);

	free(dataToHash);

	/* compute KDFContext = (ZIDi || ZIDr || total_hash) and set it in the channel context */
	zrtpChannelContext->KDFContextLength = 24+zrtpChannelContext->hashLength; /* 24 for two 12 bytes ZID */
	zrtpChannelContext->KDFContext = (uint8_t *)malloc(zrtpChannelContext->KDFContextLength*sizeof(uint8_t));
	memcpy(zrtpChannelContext->KDFContext, ZIDi, 12); /* ZIDi*/
	memcpy(zrtpChannelContext->KDFContext+12, ZIDr, 12); /* ZIDr */
	memcpy(zrtpChannelContext->KDFContext+24, totalHash, zrtpChannelContext->hashLength); /* total Hash*/

	free(totalHash); /* total hash is not needed anymore, get it from KDF Context in s0 computation */

	/* compute s0 = hash(counter || DHResult || "ZRTP-HMAC-KDF" || ZIDi || ZIDr || total_hash || len(s1) || s1 || len(s2) || s2 || len(s3) || s3)
	 * counter is a fixed 32 bits integer in big endian set to 1 : 0x00000001
	 */

	/* get s1 from rs1 or rs2 */
	if (zrtpContext->cachedSecret.rs1 != NULL) { /* if there is a s1 (already validated when received the DHpacket) */
		s1 = zrtpContext->cachedSecret.rs1;
		s1Length = zrtpContext->cachedSecret.rs1Length;
	} else if (zrtpContext->cachedSecret.rs2 != NULL) { /* otherwise if there is a s2 (already validated when received the DHpacket) */
		s1 = zrtpContext->cachedSecret.rs2;
		s1Length = zrtpContext->cachedSecret.rs2Length;
	}

	/* s2 is the auxsecret */
	s2 = zrtpContext->cachedSecret.auxsecret; /* this may be null if no match or no aux secret where found */
	s2Length = zrtpContext->cachedSecret.auxsecretLength; /* this may be 0 if no match or no aux secret where found */

	/* s3 is the pbxsecret */
	s3 = zrtpContext->cachedSecret.pbxsecret; /* this may be null if no match or no pbx secret where found */
	s3Length = zrtpContext->cachedSecret.pbxsecretLength; /* this may be 0 if no match or no pbx secret where found */

	uint16_t sharedSecretLength = bzrtp_computeKeyAgreementSharedSecretLength(zrtpChannelContext->keyAgreementAlgo);
	hashDataLength = 4/*counter*/ + sharedSecretLength/*DHResult*/+13/*ZRTP-HMAC-KDF string*/ + 12/*ZIDi*/ + 12/*ZIDr*/ + zrtpChannelContext->hashLength/*total_hash*/ + 4/*len(s1)*/ +s1Length/*s1*/ + 4/*len(s2)*/ +s2Length/*s2*/ + 4/*len(s3)*/ + s3Length/*s3*/;

	dataToHash = (uint8_t *)malloc(hashDataLength*sizeof(uint8_t));
	/* counter */
	dataToHash[0] = 0x00;
	dataToHash[1] = 0x00;
	dataToHash[2] = 0x00;
	dataToHash[3] = 0x01;
	hashDataIndex = 4;

	if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH2k || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH3k) {
		bctbx_DHMContext_t *DHMContext = (bctbx_DHMContext_t *)zrtpContext->keyAgreementContext;
		memcpy(dataToHash+hashDataIndex, DHMContext->key, sharedSecretLength);
	}
	if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X255 || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448) {
		bctbx_ECDHContext_t *ECDHContext = (bctbx_ECDHContext_t *)zrtpContext->keyAgreementContext;
		memcpy(dataToHash+hashDataIndex, ECDHContext->sharedSecret, sharedSecretLength);
	}

	hashDataIndex += sharedSecretLength;
	memcpy(dataToHash+hashDataIndex, "ZRTP-HMAC-KDF", 13);
	hashDataIndex += 13;
	/* KDF Context is already ZIDi || ZIDr || total_hash use it directly */
	memcpy(dataToHash+hashDataIndex, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength);
	hashDataIndex += zrtpChannelContext->KDFContextLength;

	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s1Length&0xFF);
	if (s1!=NULL) {
		memcpy(dataToHash+hashDataIndex, s1, s1Length);
		hashDataIndex += s1Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s2Length&0xFF);
	if (s2!=NULL) {
		memcpy(dataToHash+hashDataIndex, s2, s2Length);
		hashDataIndex += s2Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s3Length&0xFF);
	if (s3!=NULL) {
		memcpy(dataToHash+hashDataIndex, s3, s3Length);
		hashDataIndex += s3Length;
	}

	/* clean local s1,s2,s3 pointers as they are not needed anymore */
	s1=NULL;
	s2=NULL;
	s3=NULL;

	zrtpChannelContext->s0 = (uint8_t *)malloc(zrtpChannelContext->hashLength*sizeof(uint8_t));
	zrtpChannelContext->hashFunction(dataToHash, hashDataLength, zrtpChannelContext->hashLength, zrtpChannelContext->s0);

	free(dataToHash);

	/* now compute the ZRTPSession key : section 4.5.2
	 * ZRTPSess = KDF(s0, "ZRTP Session Key", KDF_Context, negotiated hash length)*/
	zrtpContext->ZRTPSessLength=zrtpChannelContext->hashLength; /* must be set to the length of negotiated hash */
	zrtpContext->ZRTPSess = (uint8_t *)malloc(zrtpContext->ZRTPSessLength*sizeof(uint8_t));
	bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength,
		(uint8_t *)"ZRTP Session Key", 16,
		zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength,
		zrtpChannelContext->hashLength,
		zrtpChannelContext->hmacFunction,
		zrtpContext->ZRTPSess);

	/* clean the DHM context (secret and key shall be erased by this operation) */
	if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH2k || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_DH3k) {
		bctbx_DestroyDHMContext((bctbx_DHMContext_t *)(zrtpContext->keyAgreementContext));
	}
	if (zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X255 || zrtpContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448) {
		bctbx_DestroyECDHContext((bctbx_ECDHContext_t *)(zrtpContext->keyAgreementContext));
	}
	zrtpContext->keyAgreementContext = NULL;
	zrtpContext->keyAgreementAlgo = ZRTP_UNSET_ALGO;

	/* now derive the other keys */
	return bzrtp_deriveKeysFromS0(zrtpContext, zrtpChannelContext);
}

/**
 * @brief In multistream mode, when we must send a confirm1 or receive a confirm1 for the first time, call the function to compute
 * s0, KDF context and derive mac and srtp keys
 *
 * param[in]	zrtpContext			The context we are operation on(where to find the ZRTPSess)
 * param[in]	zrtpChannelContext	The channel context we are operation on
 *
 * return 0 on success, error code otherwise
 */
int bzrtp_computeS0MultiStreamMode(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {
	uint8_t *dataToHash; /* a buffer used to store concatened data to be hashed */
	uint16_t hashDataLength; /* Length of the buffer */
	uint16_t hashDataIndex; /* an index used while filling the buffer */

	uint8_t *ZIDi; /* a pointer to the 12 bytes string initiator's ZID */
	uint8_t *ZIDr; /* a pointer to the 12 bytes string responder's ZID */

	uint8_t *totalHash;

	int retval;

	/* compute the total hash as in rfc section 4.4.3.2 total_hash = hash(Hello of responder || Commit) */
	if (zrtpChannelContext->role == BZRTP_ROLE_RESPONDER) { /* if we are responder */
		hashDataLength = zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength + zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
		dataToHash = (uint8_t *)malloc(hashDataLength*sizeof(uint8_t));
		hashDataIndex = 0;

		memcpy(dataToHash, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex += zrtpChannelContext->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);

		ZIDi = zrtpContext->peerZID;
		ZIDr = zrtpContext->selfZID;
	} else { /* if we are initiator */
		hashDataLength = zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength + zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
		dataToHash = (uint8_t *)malloc(hashDataLength*sizeof(uint8_t));
		hashDataIndex = 0;

		memcpy(dataToHash, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
		hashDataIndex += zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
		memcpy(dataToHash+hashDataIndex, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);

		ZIDi = zrtpContext->selfZID;
		ZIDr = zrtpContext->peerZID;
	}

	totalHash = (uint8_t *)malloc(zrtpChannelContext->hashLength);
	zrtpChannelContext->hashFunction(dataToHash, hashDataLength, zrtpChannelContext->hashLength, totalHash);

	free(dataToHash);

	/* compute KDFContext = (ZIDi || ZIDr || total_hash) and set it in the channel context */
	zrtpChannelContext->KDFContextLength = 24+zrtpChannelContext->hashLength; /* 24 for two 12 bytes ZID */
	zrtpChannelContext->KDFContext = (uint8_t *)malloc(zrtpChannelContext->KDFContextLength*sizeof(uint8_t));
	memcpy(zrtpChannelContext->KDFContext, ZIDi, 12); /* ZIDi*/
	memcpy(zrtpChannelContext->KDFContext+12, ZIDr, 12); /* ZIDr */
	memcpy(zrtpChannelContext->KDFContext+24, totalHash, zrtpChannelContext->hashLength); /* total Hash*/

	free(totalHash); /* total hash is not needed anymore, get it from KDF Context in s0 computation */

	/* compute s0 as in rfc section 4.4.3.2  s0 = KDF(ZRTPSess, "ZRTP MSK", KDF_Context, negotiated hash length) */
	zrtpChannelContext->s0 = (uint8_t *)malloc(zrtpChannelContext->hashLength*sizeof(uint8_t));
	retval = bzrtp_keyDerivationFunction(zrtpContext->ZRTPSess, zrtpContext->ZRTPSessLength,
		(uint8_t *)"ZRTP MSK", 8,
		zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength,
		zrtpChannelContext->hashLength,
		zrtpChannelContext->hmacFunction,
		zrtpChannelContext->s0);

	if (retval != 0) {
		return retval;
	}

	/* now derive the other keys */
	return bzrtp_deriveKeysFromS0(zrtpContext, zrtpChannelContext);
}


/**
 * @brief This function is called after s0 (and ZRTPSess when non in Multistream mode) have been computed to derive the other keys
 * Keys computed are: mackeyi, mackeyr, zrtpkeyi and zrtpkeyr, srtpkeys and salt
 *
 * param[in]		zrtpContext			The context we are operation on(contains ZRTPSess)
 * param[in,out]	zrtpChannelContext	The channel context we are operation on(contains s0 and will get the computed keys)
 *
 * return 0 on success, error code otherwise
 *
 */
int bzrtp_deriveKeysFromS0(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {
	int retval = 0;
	/* allocate memory for mackeyi, mackeyr, zrtpkeyi, zrtpkeyr */
	zrtpChannelContext->mackeyi = (uint8_t *)malloc(zrtpChannelContext->hashLength*(sizeof(uint8_t)));
	zrtpChannelContext->mackeyr = (uint8_t *)malloc(zrtpChannelContext->hashLength*(sizeof(uint8_t)));
	zrtpChannelContext->zrtpkeyi = (uint8_t *)malloc(zrtpChannelContext->cipherKeyLength*(sizeof(uint8_t)));
	zrtpChannelContext->zrtpkeyr = (uint8_t *)malloc(zrtpChannelContext->cipherKeyLength*(sizeof(uint8_t)));

	/* derive the keys according to rfc section 4.5.3 */
	/* mackeyi = KDF(s0, "Initiator HMAC key", KDF_Context, negotiated hash length)*/
	retval = bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Initiator HMAC key", 18, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->hashLength, zrtpChannelContext->hmacFunction, zrtpChannelContext->mackeyi);
	/* mackeyr = KDF(s0, "Responder HMAC key", KDF_Context, negotiated hash length) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Responder HMAC key", 18, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->hashLength, zrtpChannelContext->hmacFunction, zrtpChannelContext->mackeyr);
	/* zrtpkeyi = KDF(s0, "Initiator ZRTP key", KDF_Context, negotiated AES key length) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Initiator ZRTP key", 18, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->cipherKeyLength, zrtpChannelContext->hmacFunction, zrtpChannelContext->zrtpkeyi);
	/* zrtpkeyr = KDF(s0, "Responder ZRTP key", KDF_Context, negotiated AES key length) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Responder ZRTP key", 18, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->cipherKeyLength, zrtpChannelContext->hmacFunction, zrtpChannelContext->zrtpkeyr);

	return retval;
}

/**
 * @brief This function is called after confirm1 is received by initiator or confirm2 by responder
 * Keys computed are: srtp self and peer keys and salt, SAS(if mode is not multistream).
 * The whole bzrtpSrtpSecrets_t structure is ready after this call
 *
 * param[in]		zrtpContext			The context we are operation on
 * param[in,out]	zrtpChannelContext	The channel context we are operation on(contains s0 and will get the computed keys)
 *
 * return 0 on success, error code otherwise
 *
 */
int bzrtp_deriveSrtpKeysFromS0(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {
	int retval = 0;
	/* allocate memory */
	uint8_t *srtpkeyi = (uint8_t *)malloc(zrtpChannelContext->cipherKeyLength*sizeof(uint8_t));
	uint8_t *srtpkeyr = (uint8_t *)malloc(zrtpChannelContext->cipherKeyLength*sizeof(uint8_t));
	uint8_t *srtpsalti = (uint8_t *)malloc(14*sizeof(uint8_t));/* salt length is defined to be 112 bits(14 bytes) in rfc section 4.5.3 */
	uint8_t *srtpsaltr = (uint8_t *)malloc(14*sizeof(uint8_t));/* salt length is defined to be 112 bits(14 bytes) in rfc section 4.5.3 */
	/* compute keys and salts according to rfc section 4.5.3 */
	/* srtpkeyi = KDF(s0, "Initiator SRTP master key", KDF_Context, negotiated AES key length) */
	retval = bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Initiator SRTP master key", 25, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->cipherKeyLength, zrtpChannelContext->hmacFunction, srtpkeyi);
	/* srtpsalti = KDF(s0, "Initiator SRTP master salt", KDF_Context, 112) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Initiator SRTP master salt", 26, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, 14, zrtpChannelContext->hmacFunction, srtpsalti);

	/* srtpkeyr = KDF(s0, "Responder SRTP master key", KDF_Context, negotiated AES key length) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Responder SRTP master key", 25, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpChannelContext->cipherKeyLength, zrtpChannelContext->hmacFunction, srtpkeyr);
	/* srtpsaltr = KDF(s0, "Responder SRTP master salt", KDF_Context, 112) */
	retval += bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"Responder SRTP master salt", 26, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, 14, zrtpChannelContext->hmacFunction, srtpsaltr);

	if (retval!=0) {
		free(srtpkeyi);
		free(srtpkeyr);
		free(srtpsalti);
		free(srtpsaltr);
		return retval;
	}

	/* Associate responder or initiator to self or peer. Self is used to locally encrypt and peer to decrypt */
	if (zrtpChannelContext->role == BZRTP_ROLE_INITIATOR) { /* we use keyi to encrypt and keyr to decrypt */
		zrtpChannelContext->srtpSecrets.selfSrtpKey = srtpkeyi;
		zrtpChannelContext->srtpSecrets.selfSrtpSalt = srtpsalti;
		zrtpChannelContext->srtpSecrets.peerSrtpKey = srtpkeyr;
		zrtpChannelContext->srtpSecrets.peerSrtpSalt = srtpsaltr;
	} else { /* we use keyr to encrypt and keyi to decrypt */
		zrtpChannelContext->srtpSecrets.selfSrtpKey = srtpkeyr;
		zrtpChannelContext->srtpSecrets.selfSrtpSalt = srtpsaltr;
		zrtpChannelContext->srtpSecrets.peerSrtpKey = srtpkeyi;
		zrtpChannelContext->srtpSecrets.peerSrtpSalt = srtpsalti;
	}


	/* Set the length in secrets structure */
	zrtpChannelContext->srtpSecrets.selfSrtpKeyLength = zrtpChannelContext->cipherKeyLength;
	zrtpChannelContext->srtpSecrets.selfSrtpSaltLength = 14; /* salt length is defined to be 112 bits(14 bytes) in rfc section 4.5.3 */
	zrtpChannelContext->srtpSecrets.peerSrtpKeyLength = zrtpChannelContext->cipherKeyLength;
	zrtpChannelContext->srtpSecrets.peerSrtpSaltLength = 14; /* salt length is defined to be 112 bits(14 bytes) in rfc section 4.5.3 */

	/* Set the used algo in secrets structure */
	zrtpChannelContext->srtpSecrets.cipherAlgo = zrtpChannelContext->cipherAlgo;
	zrtpChannelContext->srtpSecrets.cipherKeyLength = zrtpChannelContext->cipherKeyLength;
	zrtpChannelContext->srtpSecrets.authTagAlgo = zrtpChannelContext->authTagAlgo;
	/* for information purpose, add the negotiated algorithm */
	zrtpChannelContext->srtpSecrets.hashAlgo = zrtpChannelContext->hashAlgo;
	zrtpChannelContext->srtpSecrets.keyAgreementAlgo = zrtpChannelContext->keyAgreementAlgo;
	zrtpChannelContext->srtpSecrets.sasAlgo = zrtpChannelContext->sasAlgo;


	/* compute the SAS according to rfc section 4.5.2 sashash = KDF(s0, "SAS", KDF_Context, 256) */
	if (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Mult) { /* only when not in Multistream mode */
		uint8_t sasHash[32]; /* length of hash is 256 bits -> 32 bytes */
		uint32_t sasValue;

		retval = bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength,
				(uint8_t *)"SAS", 3,
				zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength,
				32,
				zrtpChannelContext->hmacFunction,
				sasHash);
		if (retval!=0) {
			return retval;
		}

		/* now get it into a char according to the selected algo */
		sasValue = ((uint32_t)sasHash[0]<<24) | ((uint32_t)sasHash[1]<<16) | ((uint32_t)sasHash[2]<<8) | ((uint32_t)(sasHash[3]));
		zrtpChannelContext->srtpSecrets.sasLength = zrtpChannelContext->sasLength;
		zrtpChannelContext->srtpSecrets.sas = (char *)malloc((zrtpChannelContext->sasLength)*sizeof(char)); /*this shall take in account the selected representation algo for SAS */

		zrtpChannelContext->sasFunction(sasValue, zrtpChannelContext->srtpSecrets.sas, zrtpChannelContext->sasLength);

		/* set also the cache mismtach flag in srtpSecrets structure, may occurs only on the first channel */
		if (zrtpContext->cacheMismatchFlag!=0) {
			zrtpChannelContext->srtpSecrets.cacheMismatch = 1;
		}
	}

	return 0;
}

/*
 * @brief Compute the new rs1 and update the cached secrets according to rfc section 4.6.1
 *
 * param[in]		zrtpContext			The context we are operation on
 * param[in,out]	zrtpChannelContext	The channel context we are operation on(contains s0)
 *
 * return 0 on success, error code otherwise
 */
int bzrtp_updateCachedSecrets(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {
	const char *colNames[] = {"rs1", "rs2"};
	uint8_t *colValues[2] = {NULL, NULL};
	size_t colLength[2] = {RETAINED_SECRET_LENGTH,0};

	/* if this channel context is in multistream mode, do nothing */
	if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult) {
		/* destroy s0 */
		bzrtp_DestroyKey(zrtpChannelContext->s0, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
		free(zrtpChannelContext->s0);
		zrtpChannelContext->s0 = NULL;
		return 0;
	}

	/* if we had a cache mismatch, the update must be delayed until Sas confirmation by user, just do nothing, the update function will be called again when done */
	if (zrtpContext->cacheMismatchFlag == 1) {
		return 0;
	}

	/* if this channel context is in DHM mode, backup rs1 in rs2 if it exists */
	if (zrtpChannelContext->keyAgreementAlgo != ZRTP_KEYAGREEMENT_Prsh) {
		if (zrtpContext->cachedSecret.rs1 != NULL) { /* store rs2 pointer and length to be passed to cache_write function (otherwise, keep NULL and the rs2 in cache will be erased) */
			colValues[1] = zrtpContext->cachedSecret.rs1;
			colLength[1] = RETAINED_SECRET_LENGTH;
		}
	}

	/* compute rs1  = KDF(s0, "retained secret", KDF_Context, 256) */
	zrtpContext->cachedSecret.rs1 = (uint8_t *)malloc(RETAINED_SECRET_LENGTH); /* Allocate a new buffer for rs1, the old one if exists if still pointed at by colValues[1] */
	zrtpContext->cachedSecret.rs1Length = RETAINED_SECRET_LENGTH;

	bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, (uint8_t *)"retained secret", 15, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, RETAINED_SECRET_LENGTH, zrtpChannelContext->hmacFunction, zrtpContext->cachedSecret.rs1);

	colValues[0]=zrtpContext->cachedSecret.rs1;

	/* before writing into cache, we must check we have the zuid correctly set, if not (it's our first successfull exchange with peer), insert it*/
	if (zrtpContext->zuid==0) {
		bzrtp_cache_getZuid((void *)zrtpContext->zidCache, zrtpContext->selfURI, zrtpContext->peerURI, zrtpContext->peerZID, BZRTP_ZIDCACHE_INSERT_ZUID, &zrtpContext->zuid, zrtpContext->zidCacheMutex);
	}

	bzrtp_cache_write_active(zrtpContext, "zrtp", colNames, colValues, colLength, 2);

	/* if exist, call the callback function to perform custom cache operation that may use s0(writing exported key into cache) */
	if (zrtpContext->zrtpCallbacks.bzrtp_contextReadyForExportedKeys != NULL) {
		zrtpContext->zrtpCallbacks.bzrtp_contextReadyForExportedKeys(zrtpChannelContext->clientData, zrtpContext->zuid, zrtpChannelContext->role);

		/* destroy exportedKey if we computed one */
		if (zrtpContext->exportedKey!=NULL) {
			bzrtp_DestroyKey(zrtpContext->exportedKey, zrtpContext->exportedKeyLength, zrtpContext->RNGContext);
			free(zrtpContext->exportedKey);
			zrtpContext->exportedKey=NULL;
		}
	}
	/* destroy s0 */
	bzrtp_DestroyKey(zrtpChannelContext->s0, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	free(zrtpChannelContext->s0);
	zrtpChannelContext->s0 = NULL;

	/* destroy all cached keys in context they are not needed anymore (multistream mode doesn't use them to compute s0) */
	if (colValues[1] != NULL) { /* destroy the old rs1 whose pointer was saved in colValues[1] before secrets.rs1 being crushed by the new rs1 */
		bzrtp_DestroyKey(colValues[1], zrtpContext->cachedSecret.rs1Length, zrtpContext->RNGContext);
		free(colValues[1]);
		colValues[1]=NULL;
	}
	if (zrtpContext->cachedSecret.rs1!=NULL) {
		bzrtp_DestroyKey(zrtpContext->cachedSecret.rs1, zrtpContext->cachedSecret.rs1Length, zrtpContext->RNGContext);
		free(zrtpContext->cachedSecret.rs1);
		zrtpContext->cachedSecret.rs1 = NULL;
	}
	if (zrtpContext->cachedSecret.rs2!=NULL) {
		bzrtp_DestroyKey(zrtpContext->cachedSecret.rs2, zrtpContext->cachedSecret.rs2Length, zrtpContext->RNGContext);
		free(zrtpContext->cachedSecret.rs2);
		zrtpContext->cachedSecret.rs2 = NULL;
	}
	if (zrtpContext->cachedSecret.auxsecret!=NULL) {
		bzrtp_DestroyKey(zrtpContext->cachedSecret.auxsecret, zrtpContext->cachedSecret.auxsecretLength, zrtpContext->RNGContext);
		free(zrtpContext->cachedSecret.auxsecret);
		zrtpContext->cachedSecret.auxsecret = NULL;
	}
	if (zrtpContext->cachedSecret.pbxsecret!=NULL) {
		bzrtp_DestroyKey(zrtpContext->cachedSecret.pbxsecret, zrtpContext->cachedSecret.pbxsecretLength, zrtpContext->RNGContext);
		free(zrtpContext->cachedSecret.pbxsecret);
		zrtpContext->cachedSecret.pbxsecret = NULL;
	}

	return 0;
}
