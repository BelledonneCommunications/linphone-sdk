/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#ifndef asyncrw_h
#define asyncrw_h

#ifndef _WIN32_CE
#include <sys/types.h>
#endif

#include <bctoolbox/vfs.h>

typedef struct _MSAsyncReader MSAsyncReader;
typedef struct _MSAsyncWriter MSAsyncWriter;

MSAsyncReader *ms_async_reader_new(bctbx_vfs_file_t *fp);

void ms_async_reader_destroy(MSAsyncReader *obj);

int ms_async_reader_read(MSAsyncReader *obj, uint8_t *buf, size_t size);

void ms_async_reader_seek(MSAsyncReader *obj, off_t offset);


MSAsyncWriter *ms_async_writer_new(bctbx_vfs_file_t *fp);

void ms_async_writer_destroy(MSAsyncWriter *obj);

int ms_async_writer_write(MSAsyncWriter *obj, mblk_t *m);


#endif
