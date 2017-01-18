/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2014  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_internal.h"
#include "bctoolbox/vfs.h"

#ifdef HAVE_ZLIB
#include "zlib.h"
#endif

/*
 * Body handler base class implementation
 */

struct belle_sip_body_handler{
	belle_sip_object_t base;
	belle_sip_body_handler_progress_callback_t progress_cb;
	size_t expected_size; /* 0 if unknown*/
	size_t transfered_size;
	belle_sip_list_t *headers; /**> used when this body is part of a multipart message to store the header of this part */
	char *headerStringBuffer; /**> buffer populated with a string created from marshaling the headers */
	void *user_data;
};

static void belle_sip_body_handler_clone(belle_sip_body_handler_t *obj, const belle_sip_body_handler_t *orig){
	obj->progress_cb=orig->progress_cb;
	obj->user_data=orig->user_data;
	obj->expected_size=orig->expected_size;
	obj->transfered_size=orig->transfered_size;
	obj->headers=belle_sip_list_copy_with_data(orig->headers,(void *(*)(void*))belle_sip_object_clone_and_ref);
	if (orig->headerStringBuffer!=NULL) {
		obj->headerStringBuffer = strdup(orig->headerStringBuffer);
	}
}

static void belle_sip_body_handler_destroy(belle_sip_body_handler_t *obj){
	belle_sip_list_free_with_data(obj->headers,belle_sip_object_unref);
	belle_sip_free(obj->headerStringBuffer);
}

static belle_sip_error_code belle_sip_body_handler_marshal(belle_sip_body_handler_t *obj, char *buff, size_t buff_size, size_t *offset) {
	int ret;
	size_t len;
	if (*offset == 0) belle_sip_body_handler_begin_send_transfer(obj);
	do {
		len = buff_size - *offset;
		ret = belle_sip_body_handler_send_chunk(obj, NULL, (uint8_t*)buff + *offset, &len);
		*offset += len;
	} while ((ret == BELLE_SIP_CONTINUE) && (len > 0));
	if ((ret == BELLE_SIP_CONTINUE) || (ret == BELLE_SIP_BUFFER_OVERFLOW)) return BELLE_SIP_BUFFER_OVERFLOW;
	if (ret == BELLE_SIP_STOP) belle_sip_body_handler_end_transfer(obj);
	return BELLE_SIP_OK;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_body_handler_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_body_handler_t)
	{
		BELLE_SIP_VPTR_INIT(belle_sip_body_handler_t,belle_sip_object_t,TRUE),
		(belle_sip_object_destroy_t) belle_sip_body_handler_destroy,
		(belle_sip_object_clone_t) belle_sip_body_handler_clone,
		(belle_sip_object_marshal_t) belle_sip_body_handler_marshal,
		BELLE_SIP_DEFAULT_BUFSIZE_HINT
	},
	NULL, /* begin_recv_transfer */
	NULL, /* begin_send_transfer */
	NULL, /* end_transfer */
	NULL, /*chunk_recv*/
	NULL /*chunk_send*/
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

void belle_sip_body_handler_init(belle_sip_body_handler_t *obj, belle_sip_body_handler_progress_callback_t progress_cb, void *user_data){
	obj->user_data=user_data;
	obj->progress_cb=progress_cb;
	obj->headers = NULL; /* header is not used in most of the case, set it using a dedicated function if needed */
	obj->headerStringBuffer = NULL; /* header string buffer is set when adding a body handler to a multipart body handler */
}

void belle_sip_body_handler_add_header(belle_sip_body_handler_t *obj, belle_sip_header_t *header) {
	if (header != NULL) {
		obj->headers=belle_sip_list_append(obj->headers,belle_sip_object_ref(header));
	}
}

void belle_sip_body_handler_remove_header_from_ptr(belle_sip_body_handler_t *obj, belle_sip_header_t* header) {
	belle_sip_list_t* it = belle_sip_list_find(obj->headers, header);
	if (it) {
		belle_sip_object_unref(header);
		obj->headers = belle_sip_list_delete_link(obj->headers, it);
	}
}

const belle_sip_list_t* belle_sip_body_handler_get_headers(const belle_sip_body_handler_t *obj) {
	return obj->headers;
}

size_t belle_sip_body_handler_get_size(const belle_sip_body_handler_t *obj){
	return obj->expected_size;
}

void belle_sip_body_handler_set_size(belle_sip_body_handler_t *obj, size_t size){
	obj->expected_size=size;
}

size_t belle_sip_body_handler_get_transfered_size(const belle_sip_body_handler_t *obj){
	return obj->transfered_size;
}

static void update_progress(belle_sip_body_handler_t *obj, belle_sip_message_t *msg){
	if (obj->progress_cb)
		obj->progress_cb(obj,msg,obj->user_data,obj->transfered_size,obj->expected_size);
}

void belle_sip_body_handler_begin_recv_transfer(belle_sip_body_handler_t *obj){
	BELLE_SIP_OBJECT_VPTR_TYPE(belle_sip_body_handler_t) *vptr = BELLE_SIP_OBJECT_VPTR(obj, belle_sip_body_handler_t);
	if (vptr->begin_recv_transfer != NULL) {
		vptr->begin_recv_transfer(obj);
	}
	obj->transfered_size=0;
}

void belle_sip_body_handler_begin_send_transfer(belle_sip_body_handler_t *obj) {
	BELLE_SIP_OBJECT_VPTR_TYPE(belle_sip_body_handler_t) *vptr = BELLE_SIP_OBJECT_VPTR(obj, belle_sip_body_handler_t);
	if (vptr->begin_send_transfer != NULL) {
		vptr->begin_send_transfer(obj);
	}
	obj->transfered_size = 0;
}

void belle_sip_body_handler_recv_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, uint8_t *buf, size_t size){
	BELLE_SIP_OBJECT_VPTR(obj,belle_sip_body_handler_t)->chunk_recv(obj,msg,(off_t)obj->transfered_size,buf,size);
	obj->transfered_size+=size;
	update_progress(obj,msg);
}

int belle_sip_body_handler_send_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, uint8_t *buf, size_t *size){
	int ret;
	size_t to_send = *size;
	if (obj->expected_size!=0){
		to_send=MIN(*size,obj->expected_size-obj->transfered_size);
	}
	ret=BELLE_SIP_OBJECT_VPTR(obj,belle_sip_body_handler_t)->chunk_send(obj,msg,(off_t)obj->transfered_size,buf,&to_send);
	obj->transfered_size+=to_send;
	*size=to_send;
	update_progress(obj,msg);
	if (obj->expected_size!=0){
		if (obj->transfered_size==obj->expected_size)
			return BELLE_SIP_STOP;
		if (ret==BELLE_SIP_STOP && obj->transfered_size<obj->expected_size){
			belle_sip_error("body handler [%p] transfered only [%i] bytes while [%i] were expected",obj,
					(int)obj->transfered_size,(int)obj->expected_size);
		}
	}
	return ret;
}

void belle_sip_body_handler_end_transfer(belle_sip_body_handler_t *obj){
	BELLE_SIP_OBJECT_VPTR_TYPE(belle_sip_body_handler_t) *vptr = BELLE_SIP_OBJECT_VPTR(obj, belle_sip_body_handler_t);
	if (vptr->end_transfer != NULL) {
		vptr->end_transfer(obj);
	}
	if (obj->expected_size==0)
		obj->expected_size=obj->transfered_size;
}

/*
 * memory body handler implementation.
**/

struct belle_sip_memory_body_handler{
	belle_sip_body_handler_t base;
	uint8_t *buffer;
	uint8_t encoding_applied;
};

static void belle_sip_memory_body_handler_destroy(belle_sip_memory_body_handler_t *obj){
	if (obj->buffer) belle_sip_free(obj->buffer);
}

static void belle_sip_memory_body_handler_clone(belle_sip_memory_body_handler_t *obj, const belle_sip_memory_body_handler_t *orig){
	if (orig->buffer) {
		obj->buffer=belle_sip_malloc(orig->base.expected_size+1);
		memcpy(obj->buffer,orig->buffer,orig->base.expected_size);
		obj->buffer[orig->base.expected_size]='\0';
	}
	obj->encoding_applied = orig->encoding_applied;
}

static void belle_sip_memory_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t size){
	belle_sip_memory_body_handler_t *obj=(belle_sip_memory_body_handler_t*)base;
	obj->buffer=belle_sip_realloc(obj->buffer,offset+size+1);
	memcpy(obj->buffer+offset,buf,size);
	obj->buffer[offset+size]='\0';
}

static int belle_sip_memory_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t *size){
	belle_sip_memory_body_handler_t *obj=(belle_sip_memory_body_handler_t*)base;
	size_t to_send=MIN(*size,obj->base.expected_size-offset);
	if (obj->buffer == NULL) return BELLE_SIP_STOP;
	memcpy(buf,obj->buffer+offset,to_send);
	*size=to_send;
	return (obj->base.expected_size-offset==*size) ? BELLE_SIP_STOP : BELLE_SIP_CONTINUE;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_memory_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_memory_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_memory_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_memory_body_handler_destroy,
			(belle_sip_object_clone_t)belle_sip_memory_body_handler_clone,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		NULL,
		NULL,
		NULL,
		belle_sip_memory_body_handler_recv_chunk,
		belle_sip_memory_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

void *belle_sip_memory_body_handler_get_buffer(const belle_sip_memory_body_handler_t *obj){
	return obj->buffer;
}

void belle_sip_memory_body_handler_set_buffer(belle_sip_memory_body_handler_t *obj, void *buffer) {
	if (obj->buffer != NULL) belle_sip_free(obj->buffer);
	obj->buffer = (uint8_t *)buffer;
}

#define BELLE_SIP_MEMORY_BODY_HANDLER_ZLIB_INITIAL_SIZE 2048

void belle_sip_memory_body_handler_apply_encoding(belle_sip_memory_body_handler_t *obj, const char *encoding) {
	if ((obj->buffer == NULL) || (obj->encoding_applied == TRUE)) return;

#ifdef HAVE_ZLIB
	if (strcmp(encoding, "deflate") == 0) {
		z_stream strm;
		size_t initial_size = belle_sip_body_handler_get_size(BELLE_SIP_BODY_HANDLER(obj));
		size_t final_size;
		unsigned int avail_out = BELLE_SIP_MEMORY_BODY_HANDLER_ZLIB_INITIAL_SIZE;
		unsigned int outbuf_size = avail_out;
		unsigned char *outbuf = belle_sip_malloc(outbuf_size);
		unsigned char *outbuf_ptr = outbuf;
		int ret;

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
		if (ret != Z_OK) return;
		strm.avail_in = (uInt)initial_size;
		strm.next_in = obj->buffer;
		do {
			if (avail_out < BELLE_SIP_MEMORY_BODY_HANDLER_ZLIB_INITIAL_SIZE) {
				unsigned int cursize = (unsigned int)(outbuf_ptr - outbuf);
				outbuf_size *= 2;
				outbuf = belle_sip_realloc(outbuf, outbuf_size);
				outbuf_ptr = outbuf + cursize;
			}
			strm.avail_out = avail_out;
			strm.next_out = outbuf_ptr;
			deflate(&strm, Z_FINISH);
			outbuf_ptr += avail_out - strm.avail_out;
			avail_out = outbuf_size - (unsigned int)(outbuf_ptr - outbuf);
		} while (strm.avail_out == 0);
		deflateEnd(&strm);
		final_size = outbuf_ptr - outbuf;
		belle_sip_message("Body has been compressed: %u->%u:\n%s", (unsigned int)initial_size, (unsigned int)final_size, obj->buffer);
		belle_sip_free(obj->buffer);
		obj->buffer = outbuf;
		belle_sip_body_handler_set_size(BELLE_SIP_BODY_HANDLER(obj), final_size);
		obj->encoding_applied = TRUE;
	} else
#endif
	{
		belle_sip_warning("%s: unknown encoding '%s'", __FUNCTION__, encoding);
	}
}

int belle_sip_memory_body_handler_unapply_encoding(belle_sip_memory_body_handler_t *obj, const char *encoding) {
	if (obj->buffer == NULL) return -1;

#ifdef HAVE_ZLIB
	if (strcmp(encoding, "deflate") == 0) {
		z_stream strm;
		size_t initial_size = belle_sip_body_handler_get_size(BELLE_SIP_BODY_HANDLER(obj));
		size_t final_size;
		unsigned int avail_out = BELLE_SIP_MEMORY_BODY_HANDLER_ZLIB_INITIAL_SIZE;
		unsigned int outbuf_size = avail_out;
		unsigned char *outbuf = belle_sip_malloc(outbuf_size);
		unsigned char *outbuf_ptr = outbuf;
		bool_t outbuf_too_small = FALSE;
		int ret;

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		ret = inflateInit(&strm);
		if (ret != Z_OK) return -1;
		strm.avail_in = (uInt)initial_size;
		strm.next_in = obj->buffer;
		do {
			if ((avail_out < BELLE_SIP_MEMORY_BODY_HANDLER_ZLIB_INITIAL_SIZE) || (outbuf_too_small == TRUE)) {
				unsigned int cursize = (unsigned int)(outbuf_ptr - outbuf);
				unsigned int increase = outbuf_size;
				outbuf_size *= 2;
				outbuf = belle_sip_realloc(outbuf, outbuf_size);
				outbuf_ptr = outbuf + cursize;
				avail_out += increase;
			}
			outbuf_too_small = FALSE;
			strm.avail_out = avail_out;
			strm.next_out = outbuf_ptr;
			ret = inflate(&strm, Z_NO_FLUSH);
			switch (ret) {
				case Z_OK:
					/* Error handling for truncated input buffer. Z_STREAM_END is not returned but there is no further input content */
					if (strm.avail_out > 0) {
						belle_sip_error("%s: truncated compressed body. Cannot uncompress it...", __FUNCTION__);
						return -1;
					}
					break;
				case Z_STREAM_END:
					// Everything is ok, continue
					break;
				case Z_BUF_ERROR:
					// Ask for more output space
					outbuf_too_small = TRUE;
					break;
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
				case Z_STREAM_ERROR:
				case Z_MEM_ERROR:
				default:
					inflateEnd(&strm);
					belle_sip_free(outbuf);
					return -1;
			}
			outbuf_ptr += avail_out - strm.avail_out;
			avail_out = outbuf_size - (unsigned int)(outbuf_ptr - outbuf);
		} while (ret != Z_STREAM_END);
		inflateEnd(&strm);
		final_size = outbuf_ptr - outbuf;
		if (final_size == outbuf_size) {
			/* If not enough space get it to be able to put the final NULL character. */
			outbuf = belle_sip_realloc(outbuf, outbuf_size + 1);
			outbuf_ptr = outbuf + final_size;
		}
		*outbuf_ptr = '\0';
		belle_sip_message("Body has been uncompressed: %u->%u:\n%s", (unsigned int)initial_size, (unsigned int)final_size, outbuf);
		belle_sip_free(obj->buffer);
		obj->buffer = outbuf;
		belle_sip_body_handler_set_size(BELLE_SIP_BODY_HANDLER(obj), final_size);
		return 0;
	} else
#endif
	{
		belle_sip_warning("%s: unknown encoding '%s'", __FUNCTION__, encoding);
		return -1;
	}
}

belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new(belle_sip_body_handler_progress_callback_t cb, void *user_data){
	belle_sip_memory_body_handler_t *obj=belle_sip_object_new(belle_sip_memory_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,cb,user_data);
	return obj;
}

belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new_from_buffer(void *buffer, size_t bufsize, belle_sip_body_handler_progress_callback_t cb, void *user_data){
	belle_sip_memory_body_handler_t *obj=belle_sip_object_new(belle_sip_memory_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,cb,user_data);
	obj->buffer=(uint8_t*)buffer;
	obj->base.expected_size=bufsize;
	return obj;
}

belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new_copy_from_buffer(const void *buffer, size_t bufsize, belle_sip_body_handler_progress_callback_t cb, void *user_data){
	belle_sip_memory_body_handler_t *obj=belle_sip_object_new(belle_sip_memory_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,cb,user_data);
	obj->buffer=(uint8_t*)belle_sip_malloc(bufsize+1);
	obj->buffer[bufsize]='\0';
	obj->base.expected_size=bufsize;
	memcpy(obj->buffer,buffer,bufsize);
	return obj;
}

/*
 * User body handler implementation
 */

struct belle_sip_user_body_handler{
	belle_sip_body_handler_t base;
	belle_sip_user_body_handler_start_callback_t start_cb;
	belle_sip_user_body_handler_recv_callback_t recv_cb;
	belle_sip_user_body_handler_send_callback_t send_cb;
	belle_sip_user_body_handler_stop_callback_t stop_cb;
};

static void belle_sip_user_body_handler_begin_transfer(belle_sip_body_handler_t *base) {
	belle_sip_user_body_handler_t *obj = (belle_sip_user_body_handler_t *)base;
	if (obj->start_cb)
		obj->start_cb((belle_sip_user_body_handler_t*)base, base->user_data);
}

static void belle_sip_user_body_handler_end_transfer(belle_sip_body_handler_t *base) {
	belle_sip_user_body_handler_t *obj = (belle_sip_user_body_handler_t *)base;
	if (obj->stop_cb)
		obj->stop_cb((belle_sip_user_body_handler_t*)base, base->user_data);
}

static void belle_sip_user_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t size){
	belle_sip_user_body_handler_t *obj=(belle_sip_user_body_handler_t*)base;
	if (obj->recv_cb)
		obj->recv_cb((belle_sip_user_body_handler_t*)base, msg, base->user_data, offset, buf, size);
	else belle_sip_warning("belle_sip_user_body_handler_t ignoring received chunk.");
}

static int belle_sip_user_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t *size){
	belle_sip_user_body_handler_t *obj=(belle_sip_user_body_handler_t*)base;
	if (obj->send_cb)
		return obj->send_cb((belle_sip_user_body_handler_t*)base, msg, base->user_data, offset, buf, size);
	else belle_sip_warning("belle_sip_user_body_handler_t ignoring send chunk.");
	*size=0;
	return BELLE_SIP_STOP;
}

static void belle_sip_user_body_handler_clone(belle_sip_user_body_handler_t *obj, const belle_sip_user_body_handler_t *orig){
	obj->start_cb=orig->start_cb;
	obj->recv_cb=orig->recv_cb;
	obj->send_cb=orig->send_cb;
	obj->stop_cb=orig->stop_cb;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_user_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_user_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_user_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) NULL,
			(belle_sip_object_clone_t)belle_sip_user_body_handler_clone,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		belle_sip_user_body_handler_begin_transfer,
		belle_sip_user_body_handler_begin_transfer,
		belle_sip_user_body_handler_end_transfer,
		belle_sip_user_body_handler_recv_chunk,
		belle_sip_user_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_user_body_handler_t *belle_sip_user_body_handler_new(
	size_t total_size,
	belle_sip_body_handler_progress_callback_t progress_cb,
	belle_sip_user_body_handler_start_callback_t start_cb,
	belle_sip_user_body_handler_recv_callback_t recv_cb,
	belle_sip_user_body_handler_send_callback_t send_cb,
	belle_sip_user_body_handler_stop_callback_t stop_cb,
	void *data){
	belle_sip_user_body_handler_t * obj=belle_sip_object_new(belle_sip_user_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,progress_cb,data);
	obj->base.expected_size=total_size;
	obj->start_cb=start_cb;
	obj->recv_cb=recv_cb;
	obj->send_cb=send_cb;
	obj->stop_cb=stop_cb;
	return obj;
}


/**
 * File body handler implementation
**/

struct belle_sip_file_body_handler{
	belle_sip_body_handler_t base;
	char *filepath;
	bctbx_vfs_file_t *file;
	belle_sip_user_body_handler_t *user_bh;
};

static void belle_sip_file_body_handler_destroy(belle_sip_file_body_handler_t *obj) {
	if (obj->filepath) belle_sip_free(obj->filepath);
	if (obj->file) {
		ssize_t ret;
		ret = bctbx_file_close(obj->file);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("Can't close file %s", obj->filepath);
		}
		obj->file = NULL;
	}
	if (obj->user_bh) {
		belle_sip_object_unref(obj->user_bh);
		obj->user_bh = NULL;
	}
}

static void belle_sip_file_body_handler_clone(belle_sip_file_body_handler_t *obj, const belle_sip_file_body_handler_t *orig) {
	obj->filepath = belle_sip_strdup(orig->filepath);
	obj->file = orig->file;
	obj->user_bh = orig->user_bh;
	if (obj->user_bh) {
		belle_sip_object_ref(obj->user_bh);
	}
}

static void belle_sip_file_body_handler_begin_recv_transfer(belle_sip_body_handler_t *base) {
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	bctbx_vfs_t *vfs = bctbx_vfs_get_default();

	if (obj->filepath == NULL) return;
	obj->file = bctbx_file_open(vfs, obj->filepath, "r+");
	if (!obj->file) {
		bctbx_error("Can't open file %s", obj->filepath);
	}
	
	if (obj->user_bh && obj->user_bh->start_cb) {
		obj->user_bh->start_cb((belle_sip_user_body_handler_t*)&(obj->user_bh->base), obj->user_bh->base.user_data);
	}
}

static void belle_sip_file_body_handler_begin_send_transfer(belle_sip_body_handler_t *base) {
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	bctbx_vfs_t *vfs = bctbx_vfs_get_default();

	if (obj->filepath == NULL) return;
	obj->file = bctbx_file_open(vfs, obj->filepath, "r");
	if (!obj->file) {
		bctbx_error("Can't open file %s", obj->filepath);
	}

	if (obj->user_bh && obj->user_bh->start_cb) {
		obj->user_bh->start_cb((belle_sip_user_body_handler_t*)&(obj->user_bh->base), obj->user_bh->base.user_data);
	}
}

static void belle_sip_file_body_handler_end_transfer(belle_sip_body_handler_t *base) {
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	
	if (obj->user_bh && obj->user_bh->stop_cb) {
		obj->user_bh->stop_cb((belle_sip_user_body_handler_t*)&(obj->user_bh->base), obj->user_bh->base.user_data);
	}
	
	if (obj->file) {
		ssize_t ret;
		ret = bctbx_file_close(obj->file);
		if (ret == BCTBX_VFS_ERROR) {
			bctbx_error("Can't close file %s", obj->filepath);
		}
		obj->file = NULL;
	}
}

static void belle_sip_file_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t size) {
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	ssize_t ret;

	if (obj->file == NULL) return;
	
	if (obj->user_bh && obj->user_bh->recv_cb) {
		obj->user_bh->recv_cb((belle_sip_user_body_handler_t*)&(obj->user_bh->base), msg, obj->user_bh->base.user_data, offset, buf, size);
	}
	
	ret = bctbx_file_write(obj->file, buf, size, offset);
	if (ret == BCTBX_VFS_ERROR) {
		bctbx_error("File body handler recv write error at offset %lu", (unsigned long)offset);
	}
}

static int belle_sip_file_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t *size) {
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	ssize_t size_t_ret;
	size_t to_send = MIN(*size, obj->base.expected_size - offset);

	if (obj->file == NULL) return BELLE_SIP_STOP;
	size_t_ret = bctbx_file_read(obj->file, buf, to_send, offset);
	if (size_t_ret == BCTBX_VFS_ERROR) {
		bctbx_error("File body handler send read error at offset %lu", (unsigned long)offset);
		return BELLE_SIP_STOP;
	}
	*size = (size_t)size_t_ret;
	
	if (obj->user_bh && obj->user_bh->send_cb) {
		int result = obj->user_bh->send_cb((belle_sip_user_body_handler_t*)&(obj->user_bh->base), msg, obj->user_bh->base.user_data, offset, buf, size);
		if (result == BELLE_SIP_STOP) return result;
	}
	
	return (((obj->base.expected_size - offset) == (size_t)size_t_ret) || (*size == 0)) ? BELLE_SIP_STOP : BELLE_SIP_CONTINUE;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_file_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_file_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_file_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_file_body_handler_destroy,
			(belle_sip_object_clone_t)belle_sip_file_body_handler_clone,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		belle_sip_file_body_handler_begin_recv_transfer,
		belle_sip_file_body_handler_begin_send_transfer,
		belle_sip_file_body_handler_end_transfer,
		belle_sip_file_body_handler_recv_chunk,
		belle_sip_file_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_file_body_handler_t *belle_sip_file_body_handler_new(const char *filepath, belle_sip_body_handler_progress_callback_t progress_cb, void *data) {
	struct stat statbuf;
	belle_sip_file_body_handler_t *obj = belle_sip_object_new(belle_sip_file_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj, progress_cb, data);
	obj->filepath = belle_sip_strdup(filepath);
	obj->user_bh = NULL;
	if (stat(obj->filepath, &statbuf) == 0) {
		obj->base.expected_size = statbuf.st_size;
	}
	return obj;
}

size_t belle_sip_file_body_handler_get_file_size(belle_sip_file_body_handler_t *file_bh) {
	return file_bh->base.expected_size;
}

void belle_sip_file_body_handler_set_user_body_handler(belle_sip_file_body_handler_t *file_bh, belle_sip_user_body_handler_t *user_bh) {
	if (file_bh) {
		file_bh->user_bh = user_bh;
		belle_sip_object_ref(file_bh->user_bh);
	}
}

/*
 * Multipart body handler implementation
 * TODO
**/

struct belle_sip_multipart_body_handler{
	belle_sip_body_handler_t base;
	belle_sip_list_t *parts;
	belle_sip_list_t *transfer_current_part;
	char *boundary;
	uint8_t *buffer;
	unsigned int related;
};
GET_SET_BOOL(belle_sip_multipart_body_handler,related,is);

static void belle_sip_multipart_body_handler_destroy(belle_sip_multipart_body_handler_t *obj){
	belle_sip_list_free_with_data(obj->parts,belle_sip_object_unref);
	if (obj->buffer != NULL) belle_sip_free(obj->buffer);
	if (obj->boundary != NULL) belle_sip_free(obj->boundary);
}

static void belle_sip_multipart_body_handler_clone(belle_sip_multipart_body_handler_t *obj){
	obj->parts=belle_sip_list_copy_with_data(obj->parts,(void *(*)(void*))belle_sip_object_clone_and_ref);
}

static void belle_sip_multipart_body_handler_begin_recv_transfer(belle_sip_body_handler_t *obj) {
	const belle_sip_list_t *it;
	belle_sip_multipart_body_handler_t *obj_multipart = (belle_sip_multipart_body_handler_t *)obj;
	for (it = obj_multipart->parts; it != NULL; it = it->next) {
		belle_sip_body_handler_t *bh = BELLE_SIP_BODY_HANDLER(it->data);
		belle_sip_body_handler_begin_recv_transfer(bh);
	}
	obj_multipart->transfer_current_part = obj_multipart->parts;
}

static void belle_sip_multipart_body_handler_begin_send_transfer(belle_sip_body_handler_t *obj) {
	const belle_sip_list_t *it;
	belle_sip_multipart_body_handler_t *obj_multipart = (belle_sip_multipart_body_handler_t *)obj;
	for (it = obj_multipart->parts; it != NULL; it = it->next) {
		belle_sip_body_handler_t *bh = BELLE_SIP_BODY_HANDLER(it->data);
		belle_sip_body_handler_begin_send_transfer(bh);
	}
	obj_multipart->transfer_current_part = obj_multipart->parts;
}

static void belle_sip_multipart_body_handler_end_transfer(belle_sip_body_handler_t *obj) {
	const belle_sip_list_t *it;
	belle_sip_multipart_body_handler_t *obj_multipart = (belle_sip_multipart_body_handler_t *)obj;
	for (it = obj_multipart->parts; it != NULL; it = it->next) {
		belle_sip_body_handler_t *bh = BELLE_SIP_BODY_HANDLER(it->data);
		belle_sip_body_handler_end_transfer(bh);
	}
	obj_multipart->transfer_current_part = NULL;
}

static void belle_sip_multipart_body_handler_recv_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, off_t offset,
							uint8_t *buffer, size_t size){
	/* Store the whole buffer, the parts will be split when belle_sip_multipart_body_handler_progress_cb() is called with transfered size equal to expected size. */
	belle_sip_multipart_body_handler_t *obj_multipart = (belle_sip_multipart_body_handler_t *)obj;
	obj_multipart->buffer = belle_sip_realloc(obj_multipart->buffer,offset + size + 1);
	memcpy(obj_multipart->buffer + offset, buffer, size);
	obj_multipart->buffer[offset + size] = '\0';
}

static int belle_sip_multipart_body_handler_send_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, off_t offset,
							uint8_t *buffer, size_t *size){
	belle_sip_multipart_body_handler_t *obj_multipart=(belle_sip_multipart_body_handler_t*)obj;

	if (obj_multipart->transfer_current_part->data) { /* we have a part, get its content from handler */
		int retval = BELLE_SIP_STOP;
		size_t offsetSize = 0; /* used to store size of data added by this function and not given by the body handler of current part */
		size_t boundary_len = strlen(obj_multipart->boundary);
		belle_sip_body_handler_t *current_part = (belle_sip_body_handler_t *)obj_multipart->transfer_current_part->data;
		*size -= strlen(obj_multipart->boundary) + 8; /* just in case it will be the end of the message, ask for less characters than possible in order to be able to add the multipart message termination. 8 is for "\r\n--" and "--\r\n" */

		if (current_part->transfered_size == 0) { /* Nothing transfered yet on this part, include a separator and the header if exists */
			size_t headersSize = 0;
			offsetSize = strlen(obj_multipart->boundary) + 4; /* 4 is for "--" and "\r\n" */

			if (current_part->headerStringBuffer != NULL) {
				headersSize = strlen(current_part->headerStringBuffer);
			}

			/* check if buffer is large enough to get the whole header + separtor and at least a byte of data */
			if (*size < headersSize+offsetSize+1) {
				return BELLE_SIP_BUFFER_OVERFLOW;
			}

			/* insert separator */
			memcpy(buffer, "--", 2);
			memcpy(buffer + 2, obj_multipart->boundary, boundary_len);
			memcpy(buffer + 2 + boundary_len, "\r\n", 2);
			offsetSize = boundary_len + 4;
			/* insert part header */
			if (headersSize!=0) {
				memcpy(buffer+offsetSize, current_part->headerStringBuffer, headersSize);
				offsetSize += headersSize;
			}

			*size -=offsetSize; /* decrease data length requested to the current part handler */
		}

		retval = belle_sip_body_handler_send_chunk(current_part, msg, buffer+offsetSize, size); /* add offsetSize to the buffer address in order to point at the begining of free space (after header if included) */

		*size +=offsetSize; /* restore total of data given including potential separator and header */


		if (retval == BELLE_SIP_CONTINUE) {
			return BELLE_SIP_CONTINUE; /* there is still data to be sent, continue */
		} else { /* this part has reach the end, pass to next one if there is one */
			if (obj_multipart->transfer_current_part->next!=NULL) { /* there is an other part to be sent */
				obj_multipart->transfer_current_part = belle_sip_list_next(obj_multipart->transfer_current_part);
				return BELLE_SIP_CONTINUE;
			} else { /* there is nothing else, close the message and return STOP */
				size_t boundary_len = strlen(obj_multipart->boundary);
				memcpy(buffer + *size, "\r\n--", 4);
				memcpy(buffer + *size + 4, obj_multipart->boundary, boundary_len);
				memcpy(buffer + *size + 4 + boundary_len, "--\r\n", 4);
				*size+=boundary_len + 8;
				return BELLE_SIP_STOP;
			}
		}
	}
	return BELLE_SIP_STOP;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_multipart_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_multipart_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_multipart_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_multipart_body_handler_destroy,
			(belle_sip_object_clone_t)belle_sip_multipart_body_handler_clone,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		belle_sip_multipart_body_handler_begin_recv_transfer,
		belle_sip_multipart_body_handler_begin_send_transfer,
		belle_sip_multipart_body_handler_end_transfer,
		belle_sip_multipart_body_handler_recv_chunk,
		belle_sip_multipart_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

static void belle_sip_multipart_body_handler_set_boundary(belle_sip_multipart_body_handler_t *obj, const char *boundary) {
	if (obj->boundary != NULL) {
		belle_sip_free(obj->boundary);
	}
	if (boundary != NULL) {
		obj->boundary = belle_sip_strdup(boundary);
	} else {
		obj->boundary = belle_sip_strdup(BELLESIP_MULTIPART_BOUNDARY);
	}
}

belle_sip_multipart_body_handler_t *belle_sip_multipart_body_handler_new(belle_sip_body_handler_progress_callback_t progress_cb, void *data,
									 belle_sip_body_handler_t *first_part, const char *boundary){
	belle_sip_multipart_body_handler_t *obj=belle_sip_object_new(belle_sip_multipart_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,progress_cb,data);
	belle_sip_multipart_body_handler_set_boundary(obj, boundary);
	obj->base.expected_size = strlen(obj->boundary) + 8; /* body's length will be part length(including boundary) + multipart end. 8 is for "\r\n--" and "--\r\n" */
	if (first_part) belle_sip_multipart_body_handler_add_part(obj,first_part);
	return obj;
}

belle_sip_multipart_body_handler_t *belle_sip_multipart_body_handler_new_from_buffer(void *buffer, size_t bufsize, const char *boundary) {
	belle_sip_multipart_body_handler_t *obj_multipart = belle_sip_object_new(belle_sip_multipart_body_handler_t);
	belle_sip_body_handler_t *obj = (belle_sip_body_handler_t *)obj_multipart;
	belle_sip_body_handler_init((belle_sip_body_handler_t *)obj, belle_sip_multipart_body_handler_progress_cb, NULL);
	belle_sip_multipart_body_handler_set_boundary(obj_multipart, boundary);
	obj_multipart->base.expected_size = bufsize;
	belle_sip_body_handler_begin_recv_transfer(obj);
	belle_sip_body_handler_recv_chunk(obj, NULL, (uint8_t *)buffer, bufsize);
	belle_sip_body_handler_end_transfer(obj);
	return obj_multipart;
}

#define DEFAULT_HEADER_STRING_SIZE 512
void belle_sip_multipart_body_handler_add_part(belle_sip_multipart_body_handler_t *obj, belle_sip_body_handler_t *part){
	obj->base.expected_size+=part->expected_size+strlen(obj->boundary) + 4; /* add the separator length to the body length as each part start with a separator. 4 is for "--" and "\r\n" */
	if (part->headers != NULL) { /* there is a declared header for this part, add its length to the expected total length */
		size_t headerStringBufferSize = DEFAULT_HEADER_STRING_SIZE;
		size_t offset = 0;
		belle_sip_list_t *headerList = part->headers;
		part->headerStringBuffer = (char *)belle_sip_malloc(DEFAULT_HEADER_STRING_SIZE);

		while (headerList != NULL) {
			size_t offsetBackup=offset; /* we must backup the offset as it will be messed up by the marshal function in case of failure */
			belle_sip_error_code returnCode = belle_sip_object_marshal(headerList->data, part->headerStringBuffer, headerStringBufferSize-5, &offset); /* -5 to leave room for carriage returns */
			if (returnCode == BELLE_SIP_BUFFER_OVERFLOW) { /* increase buffer size */
				offset=offsetBackup; /* restore the offset, no data were written to the buffer */
				headerStringBufferSize+=DEFAULT_HEADER_STRING_SIZE;
				part->headerStringBuffer = (char *)belle_sip_realloc(part->headerStringBuffer, headerStringBufferSize);
			} else if (returnCode == BELLE_SIP_OK) { /* add the carriage return chars */
				part->headerStringBuffer[offset++]='\r';
				part->headerStringBuffer[offset++]='\n';
				headerList = belle_sip_list_next(headerList);
			}
		}
		part->headerStringBuffer[offset++]='\r';
		part->headerStringBuffer[offset++]='\n';
		obj->base.expected_size += offset;
		part->headerStringBuffer[offset++]='\0'; /* null terminate the buffer in order to be able to get it length later using strlen */
	}
	obj->parts=belle_sip_list_append(obj->parts,belle_sip_object_ref(part));
}
const belle_sip_list_t* belle_sip_multipart_body_handler_get_parts(const belle_sip_multipart_body_handler_t *obj) {
	return obj->parts;
}

void belle_sip_multipart_body_handler_progress_cb(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, void *user_data, size_t transfered, size_t expected_total) {
	if (transfered == expected_total) {
		/* The full multipart body has been received, we can now parse it and split the different parts,
		 * creating a belle_sip_memory_body_handler for each part and adding them to the belle_sip_multipart_body_handler
		 * parts list. */
		belle_sip_multipart_body_handler_t *obj_multipart = (belle_sip_multipart_body_handler_t *)obj;
		belle_sip_memory_body_handler_t *memorypart;
		belle_sip_header_t *header;
		uint8_t *end_part_cursor;
		uint8_t *end_headers_cursor;
		uint8_t *end_header_cursor;
		uint8_t *cursor = obj_multipart->buffer;
		char *boundary = belle_sip_strdup_printf("--%s", obj_multipart->boundary);
		if (strncmp((char *)cursor, boundary, strlen(boundary))) {
			belle_sip_warning("belle_sip_multipart_body_handler [%p]: body not starting by specified boundary '%s'", obj_multipart, obj_multipart->boundary);
			belle_sip_free(boundary);
			return;
		}
		cursor += strlen(boundary);
		do {
			if (strncmp((char *)cursor, "\r\n", 2)) {
				belle_sip_warning("belle_sip_multipart_body_handler [%p]: no new-line after boundary", obj_multipart);
				return;
			}
			cursor += 2;
			end_part_cursor = (uint8_t *)strstr((char *)cursor, boundary);
			if (end_part_cursor == NULL) {
				belle_sip_warning("belle_sip_multipart_body_handler [%p]: cannot find next boundary", obj_multipart);
				return;
			} else {
				*end_part_cursor = 0;
				end_headers_cursor = (uint8_t *)strstr((char *)cursor, "\r\n\r\n");
				if (end_headers_cursor == NULL) {
					memorypart = belle_sip_memory_body_handler_new_copy_from_buffer(cursor, strlen((char *)cursor), NULL, NULL);
				} else {
					uint8_t *begin_body_cursor = end_headers_cursor + 4;
					memorypart = belle_sip_memory_body_handler_new_copy_from_buffer(begin_body_cursor, strlen((char *)begin_body_cursor), NULL, NULL);
					do {
						end_header_cursor = (uint8_t *)strstr((char *)cursor, "\r\n");
						*end_header_cursor = 0;
						header = belle_sip_header_parse((char *)cursor);
						if (header != NULL) {
							belle_sip_body_handler_add_header(BELLE_SIP_BODY_HANDLER(memorypart), header);
						}
						cursor = end_header_cursor + 2;
					} while (end_header_cursor != end_headers_cursor);
				}
				belle_sip_multipart_body_handler_add_part(obj_multipart, BELLE_SIP_BODY_HANDLER(memorypart));
				cursor = end_part_cursor + strlen(boundary);
			}
		} while (strcmp((char *)cursor, "--\r\n"));
		belle_sip_free(boundary);
	}
}
