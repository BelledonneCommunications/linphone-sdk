/**
	@file lime_ffi-tester.c

	@author Johan Pascal

	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

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

	@note: this file is compliant with c89 but the belle-sip.h header is not so flag is set to c99.
*/

#include <bctoolbox/tester.h>
#include <bctoolbox/port.h>
/* Variable defined to get the arguments of the test command line,
 * they are duplicate of pre-existing C++ buffers defined in lime_tester-utils.cpp
 * but it is easier to implement this way as code is more readable
 *
 * They are defined outside the test on FFI_ENABLE to avoid crippling the lime-tester.cpp code with ifdef
 * So they are set even if the FFI is not enabled
 */

/* default url and ports of X3DH servers, buffers have fixed length that shall be enough for usage*/
char ffi_test_x3dh_server_url[512] = "localhost";
char ffi_test_x3dh_c25519_server_port[16] = "25519";
char ffi_test_x3dh_c448_server_port[16] = "25520";

/* default value for the timeout */
int ffi_wait_for_timeout=4000;

#ifdef FFI_ENABLED
extern uint8_t ffi_cleanDatabase;

#include "lime/lime_ffi.h"
#include <belle-sip/belle-sip.h>
#include <stdio.h>
#include <string.h>

/****
 * HTTP stack
 ****/
static belle_sip_stack_t *stack=NULL;
static belle_http_provider_t *prov=NULL;

static int http_before_all(void) {
	char ca_root_path[256];

	stack=belle_sip_stack_new(NULL);

	prov=belle_sip_stack_create_http_provider(stack,"0.0.0.0");

	belle_tls_crypto_config_t *crypto_config=belle_tls_crypto_config_new();

	sprintf(ca_root_path, "%s/data/", bc_tester_get_resource_dir_prefix());
	belle_tls_crypto_config_set_root_ca(crypto_config, ca_root_path);
	belle_http_provider_set_tls_crypto_config(prov,crypto_config);
	belle_sip_object_unref(crypto_config);

	return 0;
}

static int http_after_all(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}

/****
 * C version of some tester-utils functions and variables
 ****/
/* global counter */
static int success_counter;
static int failure_counter;

/* message */
static const char *message_pattern[] = {
	"I have come here to chew bubble gum and kick ass, and I'm all out of bubble gum.",
	"short message"
};

/* wait for a counter to reach a value or timeout to occur, gives ticks to the belle-sip stack every SLEEP_TIME */
static int wait_for(belle_sip_stack_t*s1,int* counter,int value,int timeout) {
	int retry=0;
#define SLEEP_TIME 50
	while (*counter!=value && retry++ <(timeout/SLEEP_TIME)) {
		if (s1) belle_sip_stack_sleep(s1,SLEEP_TIME);
	}
	if (*counter!=value) return FALSE;
	else return TRUE;
}

/* default value for the timeout */
static const int ffi_defaultInitialOPkBatchSize=5;

/* get random names for devices */
static const char ffi_charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

static char *makeRandomDeviceName(const char *basename) {
	char *ret = malloc(strlen(basename)+7);
	size_t i;
	strcpy(ret, basename);
	for (i=strlen(basename); i<strlen(basename)+6; i++) {
		ret[i] = ffi_charset[bctbx_random()%strlen(ffi_charset)];
	}
	ret[i] = '\0';
	return ret;
}

static void process_io_error(void *userData, const belle_sip_io_error_event_t *event) {
	lime_ffi_processX3DHServerResponse((lime_ffi_data_t)userData, 0, NULL, 0);
}


static void process_response(void *userData, const belle_http_response_event_t *event) {
	/* note: userData is directly set to the requested lime_ffi_data_t but it may hold a structure including it if needed */
	if (event->response){
		int code=belle_http_response_get_status_code(event->response);
		belle_sip_message_t *message = BELLE_SIP_MESSAGE(event->response);
		/* all raw data access functions in lime use uint8_t *, so safely cast the body pointer to it, it's just a data stream pointer anyway */
		const uint8_t *body = (const uint8_t *)belle_sip_message_get_body(message);
		int bodySize = (int)belle_sip_message_get_body_size(message);
		lime_ffi_processX3DHServerResponse((lime_ffi_data_t)userData, code, body, bodySize);
	} else {
		lime_ffi_processX3DHServerResponse((lime_ffi_data_t)userData, 0, NULL, 0);
	}
}

/* this emulate a network transmission: bob got a mailbox (2 buffers actually) where we can post/retrieve data to/from */
/* Let's just hope that no message would be more than 1024 bytes long in this test */
static uint8_t bobDRmessageMailbox[1024];
static size_t bobDRmessageMailboxSize;
static uint8_t bobCipherMessageMailbox[1024];
static size_t bobCipherMessageMailboxSize;

static void sendMessageTo(const char *recipient, const uint8_t *DRmessage, const size_t DRmessageSize, const uint8_t *cipherMessage, const size_t cipherMessageSize) {
	if (strcmp(recipient,"bob") == 0) {
		memcpy(bobDRmessageMailbox, DRmessage, DRmessageSize);
		bobDRmessageMailboxSize = DRmessageSize;
		memcpy(bobCipherMessageMailbox, cipherMessage, cipherMessageSize);
		bobCipherMessageMailboxSize = cipherMessageSize;
		return;
	}

	BC_FAIL("recipient isn't the expected bob ???");
}

static void getMessageFor(const char *recipient, uint8_t *DRmessage, size_t *DRmessageSize, uint8_t *cipherMessage, size_t *cipherMessageSize) {
	if (strcmp(recipient,"bob") == 0) {
		memcpy(DRmessage, bobDRmessageMailbox, bobDRmessageMailboxSize);
		*DRmessageSize = bobDRmessageMailboxSize;
		bobDRmessageMailboxSize = 0; /* 'clean' the mailbox */
		memcpy(cipherMessage, bobCipherMessageMailbox,bobCipherMessageMailboxSize);
		*cipherMessageSize = bobCipherMessageMailboxSize;
		bobCipherMessageMailboxSize = 0; /* 'clean' the mailbox */
		return;
	}

	BC_FAIL("recipient isn't expected bob ???");
}

/** @brief holds the data buffers where encryption output would be written
 */
typedef struct {
	lime_ffi_RecipientData_t *recipients;
	size_t recipientsSize;
	uint8_t *cipherMessage;
	size_t cipherMessageSize;
} encryptionBuffers_t;

/** @brief Post data to X3DH server.
 * Communication with X3DH server is entirely managed out of the lib lime, in this example code it is performed over HTTPS provided by belle-sip
 * Here the HTTPS stack provider prov is a static variable in global context so there is no need to capture it, it may be the case in real usage
 * This lambda prototype is defined in lime.hpp
 *
 * @param[in]	userData	pointer given when registering this callback
 * @param[in]	limeData	a pointer to an opaque structure, must be forwarded to the response callback where it is then passed back to lime
 * @param[in]	url		The URL of X3DH server
 * @param[in]	from		The local device id, used to identify user on the X3DH server, user identification and credential verification is out of lib lime scope.
 * 				Here identification is performed on test server via belle-sip authentication mechanism and providing the test user credentials
 * @param[in]	message		The data to be sent to the X3DH server
 * @param[in]	messageSize	Size of the message buffer
 *
 * Note: we do not use userData here as we access directly the http provider from a global variable but otherwise we should retrieve it using that pointer
 */
static void X3DHServerPost(void *userData, lime_ffi_data_t limeData, const char *url, const char *from, const uint8_t *message, const size_t message_size) {
	belle_http_request_listener_callbacks_t cbs;
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	belle_sip_memory_body_handler_t *bh;

  memset(&cbs,0,sizeof(belle_http_request_listener_callbacks_t));
	bh = belle_sip_memory_body_handler_new_copy_from_buffer(message, message_size, NULL, NULL);

	uri=belle_generic_uri_parse(url);

	req=belle_http_request_create("POST",
			uri,
			belle_http_header_create("User-Agent", "lime"),
			belle_http_header_create("Content-type", "x3dh/octet-stream"),
			belle_http_header_create("X-Lime-user-identity", from),
			NULL);

	belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req),BELLE_SIP_BODY_HANDLER(bh));
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;

	l=belle_http_request_listener_create_from_callbacks(&cbs, (void *)limeData);
	belle_sip_object_data_set(BELLE_SIP_OBJECT(req), "http_request_listener", l, belle_sip_object_unref); /* Ensure the listener object is destroyed when the request is destroyed */
	belle_http_provider_send_request(prov,req,l);
};

/* the status callback :
 * - when no user data is passed: just increase the success or failure global counter(we are doing a create/delete user)
 * - when user data is set: we are doing an encryption and it holds pointers to retrieve the encryption output
 */
static void statusCallbackPostMessage(void *userData, const enum lime_ffi_CallbackReturn status, const char *message) {
	if (status == lime_ffi_CallbackReturn_success) {
		success_counter++;
		/* if we have user data we're calling back from encrypt (real code shall use two different callback functions) */
		if (userData != NULL) {
			size_t i;
			/* here is the code processing the output when all went well.
			 * Send the message to recipient
			 * that function must, before returning, send or copy the data to send them later
			 * as it is likely this is our last chance to free the userData buffers
			 * In this example we know that bodDevice is in recipients[0], real code shall loop on recipients vector
			 */

			/* cast it back to encryptionBuffers */
			encryptionBuffers_t *buffers = (encryptionBuffers_t *)userData;
			/* post them to bob */
			sendMessageTo("bob", buffers->recipients[0].DRmessage, buffers->recipients[0].DRmessageSize, buffers->cipherMessage, buffers->cipherMessageSize);

			/* Bob and Alice verified each other keys before encryption, we can check that the peerStatus of Bob is trusted */
			BC_ASSERT_EQUAL(buffers->recipients[0].peerStatus, lime_ffi_PeerDeviceStatus_trusted, int, "%d");

			/* and free the user Data buffers */
			for(i=0; i<buffers->recipientsSize; i++) {
				free(buffers->recipients[i].deviceId);
				free(buffers->recipients[i].DRmessage);
			}
			free(buffers->recipients);
			free(buffers->cipherMessage);
			free(buffers);
		}
	} else {
		failure_counter++;
	}
}

/* the status callback :
 * just increase the success or failure global counter(we are doing a create/delete user)
 */
static void statusCallback(void *userData, const enum lime_ffi_CallbackReturn status, const char *message) {
	if (status == lime_ffi_CallbackReturn_success) {
		success_counter++;
	} else {
		bctbx_error("status callback got a fail: %s", message);
		failure_counter++;
	}
}

 /* Test scenario
 * - Alice and Bob register themselves on X3DH server(use randomized device Ids to allow test server to run several test in parallel)
 * - Alice and Bob exchange their public Identity Keys and set them as trusted in their local storage
 * - Alice encrypt a message for Bob (this will fetch Bob's key from server). Encryption peerStatus reflect the fact that bob's device is trusted
 * - Bob decrypt alice message, decryption return status reflect the fact that Alice is trusted
 * - Delete Alice and Bob devices
 *
 *   @param[in] curve		Lime can run with cryptographic operations based on curve25519 or curve448, set by this parameter in this test.
 *   				One X3DH server runs on one type of key and all clients must use the same
 *   @param[in]	dbBaseFilename	The local filename for users will be this base.<alice/bob>.<curve type>.sqlite3
 *   @param[x3dh_server_url]	The URL (including port) of the X3DH server
 */
static void ffi_helloworld_test(const enum lime_ffi_CurveId curve, const char *dbBaseFilename, const char *x3dh_server_url) {
	/* users databases names: baseFilename.<alice/bob>.<curve id>.sqlite3 */
	char dbFilenameAlice[512];
	char dbFilenameBob[512];
	sprintf(dbFilenameAlice, "%s.alice.%s.sqlite3", dbBaseFilename, (curve == lime_ffi_CurveId_c25519)?"C25519":"C448");
	sprintf(dbFilenameBob, "%s.bob.%s.sqlite3", dbBaseFilename, (curve == lime_ffi_CurveId_c25519)?"C25519":"C448");
	uint8_t bobUserId[] = {'b','o','b'};
	size_t bobUserIdSize = 3;

	remove(dbFilenameAlice); /* delete the database file if already exists */
	remove(dbFilenameBob); /* delete the database file if already exists */

	/* reset counters */
	success_counter = 0;
	failure_counter = 0;
	int expected_success=0;

	/* create Random devices names (in case we use a shared test server, devices id shall be the GRUU, X3DH/Lime does not connect user (sip:uri) and device (gruu)
	 * From Lime perspective, only devices exists and they must be uniquely identifies on the X3DH server.
	 */
	char *aliceDeviceId = makeRandomDeviceName("alice.");
	char *bobDeviceId = makeRandomDeviceName("bob.");

	/* create Managers : they will open/create the database given in first parameter, and use the function given in second one to communicate with server.
	 * Any application using Lime shall create one LimeManager only, even in case of multiple users managed by the application.
	 */
	lime_manager_t aliceManager, bobManager;
	lime_ffi_manager_init(&aliceManager, dbFilenameAlice, X3DHServerPost, NULL);
	lime_ffi_manager_init(&bobManager, dbFilenameBob, X3DHServerPost, NULL);

	/* create users */
	lime_ffi_create_user(aliceManager, aliceDeviceId, x3dh_server_url, curve, ffi_defaultInitialOPkBatchSize, statusCallback, NULL);
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, ++expected_success, ffi_wait_for_timeout));

	lime_ffi_create_user(bobManager, bobDeviceId, x3dh_server_url, curve, ffi_defaultInitialOPkBatchSize, statusCallback, NULL);
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, ++expected_success, ffi_wait_for_timeout));

	/************** Identity verifications ************************/
	/*  Retrieve from Managers Bob and Alice device Identity Key */
	uint8_t aliceIk[64]; /* maximum size of an ECDSA Ik shall be 57 if we're using curve 448 */
	size_t aliceIkSize = 64;
	uint8_t bobIk[64]; /* maximum size of an ECDSA Ik shall be 57 if we're using curve 448 */
	size_t bobIkSize = 64;
	BC_ASSERT_EQUAL(lime_ffi_get_selfIdentityKey(aliceManager, aliceDeviceId, aliceIk, &aliceIkSize), LIME_FFI_SUCCESS, int, "%d");
	BC_ASSERT_EQUAL(lime_ffi_get_selfIdentityKey(bobManager, bobDeviceId, bobIk, &bobIkSize), LIME_FFI_SUCCESS, int, "%d");

	/* libsignal uses fingerprints, linphone inserts the key in SDP and then build a ZRTP auxiliary secret out of it
	 * SAS validation with matching auxiliary secret confirms that keys have been exchanged correctly
	 *
	 * There is no need to provide local device reference when setting a key as all peer devices identity infornations are
	 * shared between local devices.
	 *
	 * Last parameter is the value of trust flag, it can be reset(in case of SAS reset) by calling again this function and setting it to false.
	 *
	 * This call can be performed before or after the beginning of a Lime conversation, if something is really bad happen, it will generate an exception.
	 * When calling it with true as trusted flag after a SAS validation confirms the peer identity key, if an exception is raised
	 * it MUST be reported to user as it means that all previously established Lime session with that device were actually compromised(or someone broke ZRTP)
	*/
	BC_ASSERT_EQUAL(lime_ffi_set_peerDeviceStatus(aliceManager, bobDeviceId, bobIk, bobIkSize, lime_ffi_PeerDeviceStatus_trusted), LIME_FFI_SUCCESS, int, "%d");
	BC_ASSERT_EQUAL(lime_ffi_set_peerDeviceStatus(bobManager, aliceDeviceId, aliceIk, aliceIkSize, lime_ffi_PeerDeviceStatus_trusted), LIME_FFI_SUCCESS, int, "%d");

	/************** SENDER SIDE CODE *****************************/
	/* encrypt, parameters are:
	 *      - localDeviceId to select which of the users managed by the LimeManager we shall use to perform the encryption (in our example we have only one local device).
	 *      - recipientUser: an id of the recipient user (which can hold several devices), typically its sip:uri
	 *      - RecipientData vector (see below), list all recipient devices, will hold their DR message
	 *      - plain message
	 *      - cipher message (this one must then be distributed to all recipients devices)
	 *      - a callback
	 *      Any parameter being a char * is expected to be a null terminated string

	 * [verify] before encryption we can verify that recipient identity is a trusted peer(and eventually decide to not perform the encryption if it is not)
	 * This information will be provided by the encrypt function anyway for each recipient device
	 * Here Bob's device is trusted as we just set its identity as verified
	 */
	BC_ASSERT_TRUE(lime_ffi_get_peerDeviceStatus(aliceManager, bobDeviceId) == lime_ffi_PeerDeviceStatus_trusted);

	/*** alice encrypts a message to bob, all parameters given to encrypt function are shared_ptr. ***/
	/* The encryption generates:
	 *      - one common cipher message which must be sent to all recipient devices(depends on encryption policy, message length and recipient number, it may be actually empty)
	 *      - a cipher header per recipient device, each recipient device shall receive its specific one
	 */

	/*  prepare the data: alloc memory for the recipients data */
	size_t DRmessageSize = 0;
	size_t cipherMessageSize = 0;
	/* get maximum buffer size. The returned values are maximum and both won't be reached at the same time */
	size_t message_patternSize = strlen(message_pattern[0])+1; /* get the NULL termination char too */
	lime_ffi_encryptOutBuffersMaximumSize(message_patternSize, curve, &DRmessageSize, &cipherMessageSize);

	/* these buffer must be allocated and not local variables as we must be able to retrieve it from the callback which would be out of the scope of this function */
	lime_ffi_RecipientData_t *recipients = malloc(1*sizeof(lime_ffi_RecipientData_t));
	uint8_t *cipherMessage = malloc(cipherMessageSize);

	/* prepare buffers, deviceId and DRmessage are allocted and will be freed when encryption is completed */
	recipients[0].deviceId = malloc(strlen(bobDeviceId)+1);
	strcpy(recipients[0].deviceId, bobDeviceId);
	recipients[0].peerStatus = lime_ffi_PeerDeviceStatus_unknown; /* be sure this value is not lime_ffi_PeerDeviceStatus_fail or this device will be ignored */
	recipients[0].DRmessage = malloc(DRmessageSize);
	recipients[0].DRmessageSize = DRmessageSize;

	/* this struct will hold pointers to the needed buffers during encryption, it also must be allocated */
	encryptionBuffers_t *userData = malloc(sizeof(encryptionBuffers_t));
	userData->recipients = recipients;
	userData->recipientsSize = 1;
	userData->cipherMessage = cipherMessage;
	userData->cipherMessageSize = cipherMessageSize;

	/* encrypt, the plain message here is char * but passed as a uint8_t *, it can hold any binary content(including '\0') its size is given by a separate parameter */
	BC_ASSERT_EQUAL(lime_ffi_encrypt(aliceManager, aliceDeviceId, bobUserId, bobUserIdSize, recipients, 1, (const uint8_t *const)message_pattern[0], message_patternSize, cipherMessage, &cipherMessageSize, statusCallbackPostMessage, userData, lime_ffi_EncryptionPolicy_cipherMessage), LIME_FFI_SUCCESS, int, "%d");

	/* in real sending situation, the local instance of pointers are destroyed by exiting the function where they've been declared
	 * and where we called the encrypt function. (The lime_ffi_manager_t shall instead never be destroyed until the application terminates)
	 * to simulate this, we set them to NULL
	 */
	userData = NULL;
	recipients = NULL;
	cipherMessage = NULL;
	/****** end of SENDER SIDE CODE ******************************/

	/************** SYNCHRO **************************************/
	/* this is just waiting for the callback to increase the operation_success field in counters
	 * sending ticks to the belle-sip stack in order to process messages
	 */
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, ++expected_success, ffi_wait_for_timeout));
	/****** end of  SYNCHRO **************************************/

	/************** RECIPIENT SIDE CODE **************************/
	/* retrieve message, in real situation the server shall fan-out only the part we need or client shall parse in the DRmessages to retrieve the one addressed to him.
	 * Note: here we just use the aliceDeviceId variable, in real situation, recipient shall extract from incoming message the sender's GRUU
	 * use buffer of 1024, it shall be OK in this test, real code would get messages in a way avoiding this.
	 */
	uint8_t bobReceivedDRmessage[1024];
	size_t bobReceivedDRmessageSize = 1024;
	uint8_t bobReceivedCipherMessage[1024];
	size_t bobReceivedCipherMessageSize;
	getMessageFor("bob", bobReceivedDRmessage, &bobReceivedDRmessageSize, bobReceivedCipherMessage, &bobReceivedCipherMessageSize);

	if (bobReceivedDRmessageSize>0 && bobReceivedCipherMessageSize>0) { /* we encrypted with cipherMessage policy, so we will have a cipher Message */
		/* before decryption we can verify that sender is a trusted peer,
		 * it is not really needed as this information will be provided by the decrypt function anyway
		 */
		BC_ASSERT_TRUE(lime_ffi_get_peerDeviceStatus(bobManager, aliceDeviceId) == lime_ffi_PeerDeviceStatus_trusted);

		/* decrypt */
		size_t decryptedMessageSize = (bobReceivedCipherMessageSize>bobReceivedDRmessageSize)?bobReceivedCipherMessageSize:bobReceivedDRmessageSize; /* actual ciphered message is either in cipherMessage or DRmessage, just allocated a buffer the size of the largest one of the two.*/
		/* it is the first time bob's Device is in communication with Alice's one via message
		 * but they already exchanged their identity keys so they Bob's device trust Alice's one since the first incoming message
		 */
		uint8_t *decryptedMessage = malloc(decryptedMessageSize);
		BC_ASSERT_TRUE(lime_ffi_decrypt(bobManager, bobDeviceId, bobUserId, bobUserIdSize, aliceDeviceId, bobReceivedDRmessage, bobReceivedDRmessageSize, bobReceivedCipherMessage, bobReceivedCipherMessageSize, decryptedMessage, &decryptedMessageSize) == lime_ffi_PeerDeviceStatus_trusted);

		/* check we got the original message back */
		BC_ASSERT_EQUAL((int)message_patternSize, (int)decryptedMessageSize, int, "%d");
		BC_ASSERT_TRUE(strncmp(message_pattern[0], (char *)decryptedMessage, (message_patternSize<decryptedMessageSize)?message_patternSize:decryptedMessageSize)==0);

		free(decryptedMessage);
	} else { /* we didn't got any message for Bob */
		BC_FAIL("we didn't got any message for Bob");
	}
	/******* end of RECIPIENT SIDE CODE **************************/

	/************** Users maintenance ****************************/
	/* Around once a day the update function shall be called on LimeManagers
	 * it will perform localStorage cleanings
	 * update of cryptographic material (Signed Pre-key and One-time Pre-keys)
	 * if called more often than once a day it is ignored
	 * The update take as optionnal parameters :
	 *  - lower bound for One-time Pre-key available on server
	 *  - One-time Pre-key batch size to be generated and uploaded if lower limit on server is reached
	 *
	 * This update shall have no effect as Alice still have ffi_defaultInitialOPkBatchSize keys on X3DH server
	 */
	lime_ffi_update(aliceManager, aliceDeviceId, statusCallback, NULL, ffi_defaultInitialOPkBatchSize, 3);  /* if less than ffi_defaultInitialOPkBatchSize keys are availables on server, upload a batch of 3, typical values shall be higher. */
	/* That one instead shall upload 3 new OPks to server as we used one of Bob's keys */
	lime_ffi_update(bobManager, aliceDeviceId, statusCallback, NULL, ffi_defaultInitialOPkBatchSize, 3); /* if less than ffi_defaultInitialOPkBatchSize keys are availables on server, upload a batch of 3, typical values shall be higher. */
	/* wait for updates to complete */
	expected_success += 2;
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, expected_success, ffi_wait_for_timeout));
	/******* end of Users maintenance ****************************/

	/******* cleaning                   *************************/
	if (ffi_cleanDatabase != 0) {
		lime_ffi_delete_user(aliceManager, aliceDeviceId, statusCallback, NULL);
		lime_ffi_delete_user(bobManager, bobDeviceId, statusCallback, NULL);
		expected_success += 2;
		BC_ASSERT_TRUE(wait_for(stack, &success_counter, expected_success, ffi_wait_for_timeout));
		remove(dbFilenameAlice);
		remove(dbFilenameBob);
	}

	lime_ffi_manager_destroy(aliceManager);
	lime_ffi_manager_destroy(bobManager);

	free(aliceDeviceId);
	free(bobDeviceId);
}

static void ffi_helloworld(void) {
	char serverURL[1024];
	sprintf(serverURL, "https://%s:%s", ffi_test_x3dh_server_url, ffi_test_x3dh_c25519_server_port);
	/* run the test on Curve25519 and Curve448 based encryption if available */
#ifdef EC25519_ENABLED
	ffi_helloworld_test(lime_ffi_CurveId_c25519, "ffi_helloworld", serverURL);
#endif
#ifdef EC448_ENABLED
	sprintf(serverURL, "https://%s:%s", ffi_test_x3dh_server_url, ffi_test_x3dh_c448_server_port);
	ffi_helloworld_test(lime_ffi_CurveId_c448, "ffi_helloworld", serverURL);
#endif
}

/* This function will destroy and recreate manager given in parameter, force deleting all internal cache and start back from what is in local Storage */
static void managerClean(lime_manager_t *manager, char *db) {
	lime_ffi_manager_destroy(*manager);
	lime_ffi_manager_init(manager, db, X3DHServerPost, NULL);
}

/*
 * Allocate and set values of the lime_ffi_RecipientData_t structure
 *
 * @param[in]	DRmessageSize	size of the DRmessage to be allocated in the RecipientData structure
 * @param[in]	deviceIds	array of null terminated strings holding the recipients peer devices Id
 * @param[in]	deviceIdsSize	number of peer devices to encrypt to
 *
 * @return	the structure to be passed to encrypt
 */
static lime_ffi_RecipientData_t *allocatedRecipientBuffers(size_t DRmessageSize, char **deviceIds, size_t deviceIdsSize) {
	size_t i=0;

	lime_ffi_RecipientData_t *recipients = malloc (deviceIdsSize*sizeof(lime_ffi_RecipientData_t));

	for (i=0; i<deviceIdsSize; i++) {
		recipients[i].deviceId = malloc(strlen(deviceIds[i])+1);
		strcpy(recipients[i].deviceId, deviceIds[i]);
		recipients[i].peerStatus = lime_ffi_PeerDeviceStatus_unknown; /* be sure this value is not lime_ffi_PeerDeviceStatus_fail or this device will be ignored */
		recipients[i].DRmessage = malloc(DRmessageSize);
		recipients[i].DRmessageSize = DRmessageSize;
	}
	return recipients;
}

/**
 * free the recipients buffers
 */
static void freeRecipientBuffers(lime_ffi_RecipientData_t *recipients, size_t recipientsSize) {
	size_t i=0;
	for (i=0; i<recipientsSize; i++) {
		free(recipients[i].deviceId);
		free(recipients[i].DRmessage);
	}
	free(recipients);
}
/* Scenario
 * - create alice.d1, bob.d1 and bob.d2
 * - alice send 2 messages to Bob
 * - bob decrypt
 * - alice delete bob.d1 from known peers
 * - alice send a message to Bob
 *
 * if continuousSession is set to false, delete and recreate LimeManager before each new operation to force relying on local Storage
 */
static void ffi_basic_test(const enum lime_ffi_CurveId curve, const char *dbBaseFilename, const char *x3dh_server_url, const uint8_t continuousSession) {
	char serverUrl[256]; // needed to test set/get x3dh url server functionnality
	size_t serverUrlSize = sizeof(serverUrl);
	/* users databases names: baseFilename.<alice/bob>.<curve id>.sqlite3 */
	char dbFilenameAlice[512];
	char dbFilenameBob1[512];
	char dbFilenameBob2[512];
	sprintf(dbFilenameAlice, "%s.alice.%s.sqlite3", dbBaseFilename, (curve == lime_ffi_CurveId_c25519)?"C25519":"C448");
	sprintf(dbFilenameBob1, "%s.bob1.%s.sqlite3", dbBaseFilename, (curve == lime_ffi_CurveId_c25519)?"C25519":"C448");
	sprintf(dbFilenameBob2, "%s.bob2.%s.sqlite3", dbBaseFilename, (curve == lime_ffi_CurveId_c25519)?"C25519":"C448");
	uint8_t bobUserId[] = {'b','o','b'};
	size_t bobUserIdSize = 3;
	remove(dbFilenameAlice); /* delete the database file if already exists */
	remove(dbFilenameBob1); /* delete the database file if already exists */
	remove(dbFilenameBob2); /* delete the database file if already exists */

	/* reset counters */
	success_counter = 0;
	failure_counter = 0;
	int expected_success=0;

	/* create Random devices names (in case we use a shared test server, devices id shall be the GRUU, X3DH/Lime does not connect user (sip:uri) and device (gruu)
	 * From Lime perspective, only devices exists and they must be uniquely identifies on the X3DH server.
	 */
	char *aliceDeviceId = makeRandomDeviceName("alice.");
	char *bobDeviceId1 = makeRandomDeviceName("bob.d1.");
	char *bobDeviceId2 = makeRandomDeviceName("bob.d2.");

	/* create Managers : they will open/create the database given in first parameter, and use the function given in second one to communicate with server.
	 * Any application using Lime shall create one LimeManager only, even in case of multiple users managed by the application.
	 */
	lime_manager_t aliceManager, bobManager1, bobManager2;
	lime_ffi_manager_init(&aliceManager, dbFilenameAlice, X3DHServerPost, NULL);
	lime_ffi_manager_init(&bobManager1, dbFilenameBob1, X3DHServerPost, NULL);
	lime_ffi_manager_init(&bobManager2, dbFilenameBob2, X3DHServerPost, NULL);

	/*** Check if Alice exists in the database ***/
	BC_ASSERT_TRUE(lime_ffi_is_user(aliceManager, aliceDeviceId) == LIME_FFI_USER_NOT_FOUND);

	/*** create users ***/
	lime_ffi_create_user(aliceManager, aliceDeviceId, x3dh_server_url, curve, ffi_defaultInitialOPkBatchSize, statusCallback, NULL);
	lime_ffi_create_user(bobManager1, bobDeviceId1, x3dh_server_url, curve, ffi_defaultInitialOPkBatchSize, statusCallback, NULL);
	lime_ffi_create_user(bobManager2, bobDeviceId2, x3dh_server_url, curve, ffi_defaultInitialOPkBatchSize, statusCallback, NULL);
	expected_success +=3;
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, expected_success, ffi_wait_for_timeout));


	/* respawn managers from cache if requested */
	if (!continuousSession) {
		managerClean(&aliceManager, dbFilenameAlice);
		managerClean(&bobManager1, dbFilenameBob1);
		managerClean(&bobManager2, dbFilenameBob2);
	}

	/*** Check the device we created exists in DB ***/
	BC_ASSERT_TRUE(lime_ffi_is_user(aliceManager, aliceDeviceId) == LIME_FFI_SUCCESS);
	BC_ASSERT_TRUE(lime_ffi_is_user(bobManager1, bobDeviceId1) == LIME_FFI_SUCCESS);
	BC_ASSERT_TRUE(lime_ffi_is_user(bobManager2, bobDeviceId2) == LIME_FFI_SUCCESS);

	/*** Set/Get X3DH server URL functionality checks ***/
	/* Get alice x3dh server url */
	serverUrlSize = sizeof(serverUrl);
	lime_ffi_get_x3dhServerUrl(aliceManager, aliceDeviceId, serverUrl, &serverUrlSize);
	BC_ASSERT_TRUE(strcmp(serverUrl, x3dh_server_url) == 0);
	serverUrl[0]='\0'; // clean the serverUrl char buffer

	// Set the X3DH URL server to something else and check it worked
	lime_ffi_set_x3dhServerUrl(aliceManager, aliceDeviceId, "https://testing.testing:12345");
	serverUrlSize = sizeof(serverUrl);
	lime_ffi_get_x3dhServerUrl(aliceManager, aliceDeviceId, serverUrl, &serverUrlSize);
	BC_ASSERT_TRUE(strcmp(serverUrl,"https://testing.testing:12345") == 0);
	// Force a reload of data from local storage just to be sure the modification was perform correctly
	managerClean(&aliceManager, dbFilenameAlice);
	serverUrl[0]='\0';
	serverUrlSize = sizeof(serverUrl);
	lime_ffi_get_x3dhServerUrl(aliceManager, aliceDeviceId, serverUrl, &serverUrlSize);
	BC_ASSERT_TRUE(strcmp(serverUrl,"https://testing.testing:12345") == 0);
	// Set it back to the regular one to be able to complete the test
	lime_ffi_set_x3dhServerUrl(aliceManager, aliceDeviceId, x3dh_server_url);


	/* respawn managers from cache if requested */
	if (!continuousSession) {
		managerClean(&aliceManager, dbFilenameAlice);
		managerClean(&bobManager1, dbFilenameBob1);
		managerClean(&bobManager2, dbFilenameBob2);
	}

	/*** encrypt 2 messages to Bob at the same time, do not wait for one to be finished to encrypt the second one ***/
	/*  prepare the data: alloc memory for the recipients data */
	size_t DRmessageSize = 0;
	size_t cipherMessageSize1 = 0;
	/* get maximum buffer size. The returned values are maximum and both won't be reached at the same time */
	size_t message_patternSize1 = strlen(message_pattern[0])+1; /* get the NULL termination char too */
	lime_ffi_encryptOutBuffersMaximumSize(message_patternSize1, curve, &DRmessageSize, &cipherMessageSize1);
	char *recipientsDeviceId[] = {bobDeviceId1, bobDeviceId2};
	/* allocate recipient data buffer */
	lime_ffi_RecipientData_t *recipients1 = allocatedRecipientBuffers(DRmessageSize, recipientsDeviceId, 2);
	uint8_t *cipherMessage1 = malloc(cipherMessageSize1);

	/* same thing for the second message */
	size_t cipherMessageSize2 = 0;
	size_t message_patternSize2 = strlen(message_pattern[1])+1; /* get the NULL termination char too */
	lime_ffi_encryptOutBuffersMaximumSize(message_patternSize2, curve, &DRmessageSize, &cipherMessageSize2);
	/* allocate recipient data buffer */
	lime_ffi_RecipientData_t *recipients2 = allocatedRecipientBuffers(DRmessageSize, recipientsDeviceId, 2);
	uint8_t *cipherMessage2 = malloc(cipherMessageSize2);

	/* here we do not need user data for the status callback as we get directly the output buffer in the same function */
	BC_ASSERT_EQUAL(lime_ffi_encrypt(aliceManager, aliceDeviceId, bobUserId, bobUserIdSize, recipients1, 2, (const uint8_t *const)message_pattern[0], message_patternSize1, cipherMessage1, &cipherMessageSize1, statusCallback, NULL, lime_ffi_EncryptionPolicy_DRMessage), LIME_FFI_SUCCESS, int, "%d");
	BC_ASSERT_EQUAL(lime_ffi_encrypt(aliceManager, aliceDeviceId, bobUserId, bobUserIdSize, recipients2, 2, (const uint8_t *const)message_pattern[1], message_patternSize2, cipherMessage2, &cipherMessageSize2, statusCallback, NULL, lime_ffi_EncryptionPolicy_cipherMessage), LIME_FFI_SUCCESS, int, "%d");

	expected_success +=2;
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, expected_success, ffi_wait_for_timeout));

	/* check peer status output, they shall both be set to unknown */
	BC_ASSERT_EQUAL(recipients1[0].peerStatus, lime_ffi_PeerDeviceStatus_unknown, int, "%d" );
	BC_ASSERT_EQUAL(recipients1[1].peerStatus, lime_ffi_PeerDeviceStatus_unknown, int, "%d" );
	/* the second encrypts needs access to sessions created by the first one (as they have the same recipients
	 * so it is put in queue waiting for the first one to complete
	 * recipients are thus no more unknown but untrusted */
	BC_ASSERT_EQUAL(recipients2[0].peerStatus, lime_ffi_PeerDeviceStatus_untrusted, int, "%d" );
	BC_ASSERT_EQUAL(recipients2[1].peerStatus, lime_ffi_PeerDeviceStatus_untrusted, int, "%d" );


	/* respawn managers from cache if requested */
	if (!continuousSession) {
		managerClean(&aliceManager, dbFilenameAlice);
		managerClean(&bobManager1, dbFilenameBob1);
		managerClean(&bobManager2, dbFilenameBob2);
	}


	/***
	 * Bob devices decrypt the messages.
	 *
	 * DRmessages and Cipher Message are directly sent to bob by sharing the output buffer in the same function,
	 * hello world test is more realistic as it simulates a network transmission
	 */
	/* Bob device 1 decrypts first message */
	size_t decryptedMessageSize = (cipherMessageSize1>recipients1[0].DRmessageSize)?cipherMessageSize1:recipients1[0].DRmessageSize; /* actual ciphered message is either in cipherMessage or DRmessage, just allocated a buffer the size of the largest one of the two.*/
	uint8_t *decryptedMessage = malloc(decryptedMessageSize);
	BC_ASSERT_TRUE(lime_ffi_decrypt(bobManager1, bobDeviceId1, bobUserId, bobUserIdSize, aliceDeviceId, recipients1[0].DRmessage, recipients1[0].DRmessageSize, cipherMessage1, cipherMessageSize1, decryptedMessage, &decryptedMessageSize) == lime_ffi_PeerDeviceStatus_unknown);
	/* check we got the original message back */
	BC_ASSERT_EQUAL((int)message_patternSize1, (int)decryptedMessageSize, int, "%d");
	BC_ASSERT_TRUE(strncmp(message_pattern[0], (char *)decryptedMessage, (message_patternSize1<decryptedMessageSize)?message_patternSize1:decryptedMessageSize)==0);
	free(decryptedMessage);

	/* Bob device 1 decrypts second message */
	decryptedMessageSize = (cipherMessageSize2>recipients2[0].DRmessageSize)?cipherMessageSize2:recipients2[0].DRmessageSize;
	decryptedMessage = malloc(decryptedMessageSize);
	BC_ASSERT_TRUE(lime_ffi_decrypt(bobManager1, bobDeviceId1, bobUserId, bobUserIdSize, aliceDeviceId, recipients2[0].DRmessage, recipients2[0].DRmessageSize, cipherMessage2, cipherMessageSize2, decryptedMessage, &decryptedMessageSize) == lime_ffi_PeerDeviceStatus_untrusted);
	/* check we got the original message back */
	BC_ASSERT_EQUAL((int)message_patternSize2, (int)decryptedMessageSize, int, "%d");
	BC_ASSERT_TRUE(strncmp(message_pattern[1], (char *)decryptedMessage, (message_patternSize2<decryptedMessageSize)?message_patternSize2:decryptedMessageSize)==0);
	free(decryptedMessage);

	/* Bob's device 2 decrypts first the second message and then the first */
	/* Bob device 2 decrypts second message */
	decryptedMessageSize = (cipherMessageSize2>recipients2[1].DRmessageSize)?cipherMessageSize2:recipients2[1].DRmessageSize;
	decryptedMessage = malloc(decryptedMessageSize);
	BC_ASSERT_TRUE(lime_ffi_decrypt(bobManager2, bobDeviceId2, bobUserId, bobUserIdSize, aliceDeviceId, recipients2[1].DRmessage, recipients2[1].DRmessageSize, cipherMessage2, cipherMessageSize2, decryptedMessage, &decryptedMessageSize) == lime_ffi_PeerDeviceStatus_unknown);
	/* check we got the original message back */
	BC_ASSERT_EQUAL((int)message_patternSize2, (int)decryptedMessageSize, int, "%d");
	BC_ASSERT_TRUE(strncmp(message_pattern[1], (char *)decryptedMessage, (message_patternSize2<decryptedMessageSize)?message_patternSize2:decryptedMessageSize)==0);
	free(decryptedMessage);

	/* Bob device 2 decrypts first message */
	decryptedMessageSize = (cipherMessageSize1>recipients1[1].DRmessageSize)?cipherMessageSize1:recipients1[1].DRmessageSize;
	decryptedMessage = malloc(decryptedMessageSize);
	BC_ASSERT_TRUE(lime_ffi_decrypt(bobManager2, bobDeviceId2, bobUserId, bobUserIdSize, aliceDeviceId, recipients1[1].DRmessage, recipients1[1].DRmessageSize, cipherMessage1, cipherMessageSize1, decryptedMessage, &decryptedMessageSize) == lime_ffi_PeerDeviceStatus_untrusted);
	/* check we got the original message back */
	BC_ASSERT_EQUAL((int)message_patternSize1, (int)decryptedMessageSize, int, "%d");
	BC_ASSERT_TRUE(strncmp(message_pattern[0], (char *)decryptedMessage, (message_patternSize1<decryptedMessageSize)?message_patternSize1:decryptedMessageSize)==0);
	free(decryptedMessage);

	/* free buffers */
	freeRecipientBuffers(recipients1, 2);
	freeRecipientBuffers(recipients2, 2);
	free(cipherMessage1);
	free(cipherMessage2);


	/* respawn managers from cache if requested */
	if (!continuousSession) {
		managerClean(&aliceManager, dbFilenameAlice);
		managerClean(&bobManager1, dbFilenameBob1);
		managerClean(&bobManager2, dbFilenameBob2);
	}


	/*** alice delete bob's device 1 from known peers ***/
	BC_ASSERT_EQUAL(lime_ffi_delete_peerDevice(aliceManager, bobDeviceId1),  LIME_FFI_SUCCESS, int, "%d");

	/*** check it worked ***/
	BC_ASSERT_EQUAL(lime_ffi_get_peerDeviceStatus(aliceManager, bobDeviceId1),  lime_ffi_PeerDeviceStatus_unknown, int, "%d");

	/*** Encrypt again to bob's device 1 and 2 to check the status is back to unknown on the first one */
	DRmessageSize = 0;
	cipherMessageSize1 = 0;
	/* get maximum buffer size. The returned values are maximum and both won't be reached at the same time */
	message_patternSize1 = strlen(message_pattern[0])+1; /* get the NULL termination char too */
	lime_ffi_encryptOutBuffersMaximumSize(message_patternSize1, curve, &DRmessageSize, &cipherMessageSize1);
	/* allocate recipient data buffer */
	recipients1 = allocatedRecipientBuffers(DRmessageSize, recipientsDeviceId, 2);
	cipherMessage1 = malloc(cipherMessageSize1);

	BC_ASSERT_EQUAL(lime_ffi_encrypt(aliceManager, aliceDeviceId, bobUserId, bobUserIdSize, recipients1, 2, (const uint8_t *const)message_pattern[0], message_patternSize1, cipherMessage1, &cipherMessageSize1, statusCallback, NULL, lime_ffi_EncryptionPolicy_DRMessage), LIME_FFI_SUCCESS, int, "%d");
	BC_ASSERT_TRUE(wait_for(stack, &success_counter, ++expected_success, ffi_wait_for_timeout));
	/* check peer status output, they shall both be set to unknown */
	BC_ASSERT_EQUAL(recipients1[0].peerStatus, lime_ffi_PeerDeviceStatus_unknown, int, "%d" );
	BC_ASSERT_EQUAL(recipients1[1].peerStatus, lime_ffi_PeerDeviceStatus_untrusted, int, "%d" );

	/* free buffers */
	freeRecipientBuffers(recipients1, 2);
	free(cipherMessage1);


	/* respawn managers from cache if requested */
	if (!continuousSession) {
		managerClean(&aliceManager, dbFilenameAlice);
		managerClean(&bobManager1, dbFilenameBob1);
		managerClean(&bobManager2, dbFilenameBob2);
	}


	/******* cleaning                   *************************/
	if (ffi_cleanDatabase != 0) {
		lime_ffi_delete_user(aliceManager, aliceDeviceId, statusCallback, NULL);
		lime_ffi_delete_user(bobManager1, bobDeviceId1, statusCallback, NULL);
		lime_ffi_delete_user(bobManager2, bobDeviceId2, statusCallback, NULL);
		expected_success += 3;
		BC_ASSERT_TRUE(wait_for(stack, &success_counter, expected_success, ffi_wait_for_timeout));
		remove(dbFilenameAlice);
		remove(dbFilenameBob1);
		remove(dbFilenameBob2);
	}

	lime_ffi_manager_destroy(aliceManager);
	lime_ffi_manager_destroy(bobManager1);
	lime_ffi_manager_destroy(bobManager2);

	free(aliceDeviceId);
	free(bobDeviceId1);
	free(bobDeviceId2);
}


static void ffi_basic(void) {
	char serverURL[1024];
	sprintf(serverURL, "https://%s:%s", ffi_test_x3dh_server_url, ffi_test_x3dh_c25519_server_port);
	/* run the test on Curve25519 and Curve448 based encryption if available */
#ifdef EC25519_ENABLED
	ffi_basic_test(lime_ffi_CurveId_c25519, "ffi_basic", serverURL, 1);
	ffi_basic_test(lime_ffi_CurveId_c25519, "ffi_basic", serverURL, 0);
#endif
#ifdef EC448_ENABLED
	sprintf(serverURL, "https://%s:%s", ffi_test_x3dh_server_url, ffi_test_x3dh_c448_server_port);
	ffi_basic_test(lime_ffi_CurveId_c448, "ffi_basic", serverURL, 1);
	ffi_basic_test(lime_ffi_CurveId_c448, "ffi_basic", serverURL, 0);
#endif
}



static test_t tests[] = {
	TEST_NO_TAG("FFI Hello World", ffi_helloworld),
	TEST_NO_TAG("FFI Basic", ffi_basic)
};

test_suite_t lime_ffi_test_suite = {
	"FFI",
	http_before_all,
	http_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests,
	0
};

#else /* FFI_ENABLED */
test_suite_t lime_ffi_test_suite = {
	"FFI",
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	NULL,
	0,
	0
};

#endif /* FFI_ENABLED */
