/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016 Belledonne Communications SARL

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

#ifndef asyncrw_h
#define asyncrw_h

#ifndef _WIN32_CE
#include <sys/types.h>
#endif

typedef struct _MSAsyncReader MSAsyncReader;
typedef struct _MSAsyncWriter MSAsyncWriter;

MSAsyncReader *ms_async_reader_new(int fd);

void ms_async_reader_destroy(MSAsyncReader *obj);

int ms_async_reader_read(MSAsyncReader *obj, uint8_t *buf, size_t size);

void ms_async_reader_seek(MSAsyncReader *obj, off_t offset);


MSAsyncWriter *ms_async_writer_new(int fd);

void ms_async_writer_destroy(MSAsyncWriter *obj);

int ms_async_reader_write(MSAsyncWriter *obj, mblk_t *m);


#endif
