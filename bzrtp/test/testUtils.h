/**
 @file testUtils.c

 @author Johan Pascal

 @copyright Copyright (C) 2014 Belledonne Communications, Grenoble, France
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <stdint.h>
#include "typedef.h"
#include "packetParser.h"

void printHex(char *title, uint8_t *data, uint32_t length); 
void packetDump(bzrtpPacket_t *zrtpPacket, uint8_t addRawMessage);
void dumpContext(char *title, bzrtpContext_t *zrtpContext);

