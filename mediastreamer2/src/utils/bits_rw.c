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

#include "mediastreamer2/bits_rw.h"
#include "mediastreamer2/mscommon.h"

#include <math.h>

void ms_bits_reader_init(MSBitsReader *reader, const uint8_t *buffer, size_t bufsize){
	reader->buffer=buffer;
	reader->buf_size=bufsize;
	reader->bit_index=0;
}

int ms_bits_reader_n_bits(MSBitsReader *reader, int count, unsigned int *ret, const char* symbol_name){
	unsigned int tmp;
	size_t byte_index=reader->bit_index/8;
	size_t bit_index=reader->bit_index % 8;
	int shift=(int)(32-bit_index-count);

	if (count>=24){
		ms_error("This bit reader cannot read more than 24 bits at once.");
		return -1;
	}

	if (byte_index<reader->buf_size)
		tmp=((unsigned int)reader->buffer[byte_index++])<<24;
	else{
		ms_error("Bit reader goes end of stream.");
		return -1;
	}
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++])<<16;
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++])<<8;
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++]);

	tmp=tmp>>shift;
	tmp=tmp & ((1<<count)-1);
	reader->bit_index+=count;

	if (symbol_name) {
		ms_debug(".%s (u(%d)) : 0x%x | %u", symbol_name, count, tmp, tmp);
	}
	if (ret)
		*ret=tmp;
	return 0;
}

int ms_bits_reader_ue(MSBitsReader *reader, unsigned int* ret, const char* symbol_name){
	unsigned int trail = 0;
	unsigned int r = 0;
	int leading_zeros_cnt = -1;

	do {
		leading_zeros_cnt++;
		if (ms_bits_reader_n_bits(reader, 1, &r, 0) != 0)
			return -1;
	} while (r == 0);

	if (leading_zeros_cnt == 0) {
		if (symbol_name) {
			ms_debug(".%s (ue) : 0x%x", symbol_name, 0);
		}
		if (ret)
			*ret = 0;
	} else {
		unsigned int value;
		if (ms_bits_reader_n_bits(reader, leading_zeros_cnt, &trail, 0) != 0)
			return -1;
		value = (unsigned int)pow(2, leading_zeros_cnt) - 1 + trail;
		if (symbol_name) {
			ms_debug(".%s (ue) : 0x%x | %u", symbol_name, value, value);
		}
		if (ret)
			*ret = value;
	}

	return 0;
}

int ms_bits_reader_se(MSBitsReader *reader, int* ret, const char* symbol_name) {
	unsigned int code_num;
	int sign;
	int value;

	if (ms_bits_reader_ue(reader, &code_num, 0) != 0)
		return -1;

	sign = (code_num % 2) ? 1 : -1;
	value = (int)(sign * ceil(code_num / 2.0f));
	if (symbol_name) {
		ms_debug(".%s (se) : 0x%x | %d [%x", symbol_name, value, value, code_num);
	}
	if (ret)
		*ret = value;
	return 0;
}


void ms_bits_writer_init(MSBitsWriter *writer, size_t initialbufsize) {
	writer->buffer = (uint8_t*) malloc(initialbufsize);
	writer->buf_size = initialbufsize;
	writer->bit_index = 0;
	memset(writer->buffer, 0, writer->buf_size);
}

int ms_bits_writer_n_bits(MSBitsWriter *writer, int count, unsigned int value, const char* symbol_name) {
	uint8_t swap[4];
	int i;
	int byte_index;
	int bits_left;
	int bytes;

	for (i=0; i<4; i++) {
		swap[i] = (value >> (24 - 8 * i)) & 0xFF;
	}

	/* resize if necessary */
	if ((size_t)(writer->bit_index + count) > writer->buf_size * 8) {
		size_t old_size = writer->buf_size;
		writer->buf_size = MAX(2 * (writer->buf_size + 1), writer->buf_size + count / 8);
		writer->buffer = (uint8_t*) realloc(writer->buffer, writer->buf_size);
		memset(&writer->buffer[old_size], 0, writer->buf_size - old_size);
	}

	/* current byte */
	byte_index = writer->bit_index / 8;
	/* bits available in current byte */
	bits_left = 8 - writer->bit_index % 8;

	/* browse bytes containing bits to write */
	bytes = count == 32 ? 4 : (1 + count / 8);
	for (i=0; i < bytes; i++) {
		const uint8_t v = swap[4 - bytes + i];
		int cnt = (i == 0) ? (count - (bytes - 1) * 8) : 8;
		while (cnt) {
			if (bits_left < cnt) {
				writer->buffer[byte_index] |= v >> (cnt - bits_left);
				byte_index++;
				cnt -= bits_left;
				bits_left = 8;
			} else {
				writer->buffer[byte_index] |= v << (bits_left - cnt);
				bits_left -= cnt;
				cnt = 0;
				if (bits_left <= 0) {
					bits_left = 8;
					byte_index ++;
				}
			}
		}
	}

	writer->bit_index += count;

	return 0;
}

int ms_bits_writer_ue(MSBitsWriter *writer, unsigned int value, const char* symbol_name) {
	int size_in_bits = 0;
	int tmp_val = value + 1;

	while (tmp_val) {
		tmp_val >>= 1;
		size_in_bits++;
	}
	size_in_bits --;

	ms_bits_writer_n_bits(writer, size_in_bits, 0, NULL);
	ms_bits_writer_n_bits(writer, 1, 1, NULL);
	value = (value + 1) - (1 << size_in_bits);
	ms_bits_writer_n_bits(writer, size_in_bits, value, NULL);

	return 0;
}

int ms_bits_writer_se(MSBitsWriter *writer, int value, const char* symbol_name) {
	unsigned int new_val;

	if (value <= 0)
		new_val = -2 * value;
	else
		new_val = 2 * value - 1;

	return ms_bits_writer_ue(writer, new_val, symbol_name);
}

int ms_bits_writer_trailing_bits(MSBitsWriter *writer) {
	ms_bits_writer_n_bits(writer, 1, 1, "trailing_bits");
	// byte_aligning
	if ((writer->bit_index % 8) == 0)
		return 0;

	ms_bits_writer_n_bits(writer, 8 - (writer->bit_index % 8), 0, "byte_aligning");
	return 0;
}
