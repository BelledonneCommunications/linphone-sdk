/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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


#ifndef rfc2429_h
#define rfc2429_h

#define MAKE_MASK(bits)		( (1<<(bits)) -1 )

static inline unsigned int rfc2429_get_P(const uint8_t *header){
	return (header[0]>>2) & 0x1;
}

static inline void rfc2429_set_P(uint8_t *header, bool_t val){
	header[0]=header[0] | ( (val&0x1)<<2);
}

static inline unsigned int rfc2429_get_V(const uint8_t *header){
	return (header[0]>>1) & 0x1;
}

static inline unsigned int rfc2429_get_PLEN(const uint8_t *header){
	unsigned short *p=(unsigned short*)header;
	return (ntohs(p[0])>>3) & MAKE_MASK(6);
}

static inline unsigned int rfc2429_get_PEBIT(const uint8_t *header){
	unsigned short *p=(unsigned short*)header;
	return ntohs(p[0]) & MAKE_MASK(3);
}


#endif
