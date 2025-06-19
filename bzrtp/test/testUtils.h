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
#include <stdint.h>
#include "typedef.h"
#include "packetParser.h"

#ifdef __cplusplus
extern "C"{
#endif

extern int verbose;

void printHex(char *title, uint8_t *data, uint32_t length); 
void packetDump(bzrtpPacket_t *zrtpPacket, uint8_t addRawMessage);
void dumpContext(char *title, bzrtpContext_t *zrtpContext);

//const char *bzrtp_hash_toString(uint8_t hashAlgo);
//const char *bzrtp_keyAgreement_toString(uint8_t keyAgreementAlgo);
//const char *bzrtp_cipher_toString(uint8_t cipherAlgo);
//const char *bzrtp_authtag_toString(uint8_t authtagAlgo);
//const char *bzrtp_sas_toString(uint8_t sasAlgo);
#ifdef ZIDCACHE_ENABLED
int bzrtptester_sqlite3_open(const char *db_file, sqlite3 **db);
#endif /* ZIDCACHE_ENABLED */

#ifdef __cplusplus
};
#endif
