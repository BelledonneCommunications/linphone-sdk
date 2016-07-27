/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL
Author: Simon Morlat <simon.morlat@linphone.org>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if !defined(_BITS_RW_H_)
#define _BITS_RW_H_

#include <ortp/port.h>

typedef struct ms_bits_reader{
	const uint8_t *buffer;
	size_t buf_size;
	int bit_index;
} MSBitsReader;

#ifdef __cplusplus
extern "C" {
#endif

void ms_bits_reader_init(MSBitsReader *reader, const uint8_t *buffer, size_t bufsize);

int ms_bits_reader_n_bits(MSBitsReader *reader, int count, unsigned int *ret, const char* symbol_name);

int ms_bits_reader_ue(MSBitsReader *reader, unsigned int* ret, const char* symbol_name);

int ms_bits_reader_se(MSBitsReader *reader, int* ret, const char* symbol_name);

typedef struct ms_bits_writer {
	uint8_t* buffer;
	size_t buf_size;
	int bit_index;
} MSBitsWriter;

void ms_bits_writer_init(MSBitsWriter *writer, size_t initialbufsize);

int ms_bits_writer_n_bits(MSBitsWriter *writer, int count, unsigned int value, const char* symbol_name);

int ms_bits_writer_ue(MSBitsWriter *writer, unsigned int value, const char* symbol_name);

int ms_bits_writer_se(MSBitsWriter *writer, int value, const char* symbol_name);

int ms_bits_writer_trailing_bits(MSBitsWriter *writer);

#ifdef __cplusplus
}
#endif

#endif
