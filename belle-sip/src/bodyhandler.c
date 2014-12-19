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

void belle_sip_body_handler_add_header(belle_sip_body_handler_t *obj, belle_sip_header_t *header) {
	if (header != NULL) {
		obj->headers=belle_sip_list_append(obj->headers,belle_sip_object_ref(header));
	}
}

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

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_body_handler_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_body_handler_t)
	{
		BELLE_SIP_VPTR_INIT(belle_sip_body_handler_t,belle_sip_object_t,TRUE),
		(belle_sip_object_destroy_t) belle_sip_body_handler_destroy,
		(belle_sip_object_clone_t) belle_sip_body_handler_clone,
		NULL,/*no marshal*/
	},
	NULL, /*chunk_recv*/
	NULL /*chunk_send*/
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

void belle_sip_body_handler_init(belle_sip_body_handler_t *obj, belle_sip_body_handler_progress_callback_t progress_cb, void *user_data){
	obj->user_data=user_data;
	obj->progress_cb=progress_cb;
	obj->headers = NULL; /* header is not used in most of the case, set it using a dedicated function if needed */
	obj->headerStringBuffer = NULL; /* header string buffer is set when adding a body handler to a multipart body handler */
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

void belle_sip_body_handler_begin_transfer(belle_sip_body_handler_t *obj){
	obj->transfered_size=0;
}

void belle_sip_body_handler_recv_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, const uint8_t *buf, size_t size){
	BELLE_SIP_OBJECT_VPTR(obj,belle_sip_body_handler_t)->chunk_recv(obj,msg,obj->transfered_size,buf,size);
	obj->transfered_size+=size;
	update_progress(obj,msg);
}

int belle_sip_body_handler_send_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, uint8_t *buf, size_t *size){
	int ret;
	if (obj->expected_size!=0){
		*size=MIN(*size,obj->expected_size-obj->transfered_size);
	}
	ret=BELLE_SIP_OBJECT_VPTR(obj,belle_sip_body_handler_t)->chunk_send(obj,msg,obj->transfered_size,buf,size);
	obj->transfered_size+=*size;
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
	if (obj->expected_size==0)
		obj->expected_size=obj->transfered_size;
}

/*
 * memory body handler implementation.
**/

struct belle_sip_memory_body_handler{
	belle_sip_body_handler_t base;
	uint8_t *buffer;
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
}

static void belle_sip_memory_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, const uint8_t *buf, size_t size){
	belle_sip_memory_body_handler_t *obj=(belle_sip_memory_body_handler_t*)base;
	obj->buffer=belle_sip_realloc(obj->buffer,offset+size+1);
	memcpy(obj->buffer+offset,buf,size);
	obj->buffer[offset+size]='\0';
}

static int belle_sip_memory_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, uint8_t *buf, size_t *size){
	belle_sip_memory_body_handler_t *obj=(belle_sip_memory_body_handler_t*)base;
	size_t to_send=MIN(*size,obj->base.expected_size-offset);
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
			NULL
		},
		belle_sip_memory_body_handler_recv_chunk,
		belle_sip_memory_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

const void *belle_sip_memory_body_handler_get_buffer(const belle_sip_memory_body_handler_t *obj){
	return obj->buffer;
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
	belle_sip_user_body_handler_recv_callback_t recv_cb;
	belle_sip_user_body_handler_send_callback_t send_cb;
};

static void belle_sip_user_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, const uint8_t *buf, size_t size){
	belle_sip_user_body_handler_t *obj=(belle_sip_user_body_handler_t*)base;
	if (obj->recv_cb)
		obj->recv_cb((belle_sip_user_body_handler_t*)base, msg, base->user_data, offset, buf, size);
	else belle_sip_warning("belle_sip_user_body_handler_t ignoring received chunk.");
}

static int belle_sip_user_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, uint8_t *buf, size_t *size){
	belle_sip_user_body_handler_t *obj=(belle_sip_user_body_handler_t*)base;
	if (obj->send_cb)
		return obj->send_cb((belle_sip_user_body_handler_t*)base, msg, base->user_data, offset, buf, size);
	else belle_sip_warning("belle_sip_user_body_handler_t ignoring send chunk.");
	*size=0;
	return BELLE_SIP_STOP;
}

static void belle_sip_user_body_handler_clone(belle_sip_user_body_handler_t *obj, const belle_sip_user_body_handler_t *orig){
	obj->recv_cb=orig->recv_cb;
	obj->send_cb=orig->send_cb;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_user_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_user_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_user_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) NULL,
			(belle_sip_object_clone_t)belle_sip_user_body_handler_clone,
			NULL
		},
		belle_sip_user_body_handler_recv_chunk,
		belle_sip_user_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_user_body_handler_t *belle_sip_user_body_handler_new(
	size_t total_size,
	belle_sip_body_handler_progress_callback_t progress_cb,
	belle_sip_user_body_handler_recv_callback_t recv_cb,
	belle_sip_user_body_handler_send_callback_t send_cb,
	void *data){
	belle_sip_user_body_handler_t * obj=belle_sip_object_new(belle_sip_user_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,progress_cb,data);
	obj->base.expected_size=total_size;
	obj->recv_cb=recv_cb;
	obj->send_cb=send_cb;
	return obj;
}


/**
 * File body handler implementation
**/

struct belle_sip_file_body_handler{
	belle_sip_body_handler_t base;
	char *filepath;
};

static void belle_sip_file_body_handler_destroy(belle_sip_file_body_handler_t *obj) {
	if (obj->filepath) belle_sip_free(obj->filepath);
}

static void belle_sip_file_body_handler_clone(belle_sip_file_body_handler_t *obj, const belle_sip_file_body_handler_t *orig) {
	obj->filepath = belle_sip_strdup(orig->filepath);
}

static void belle_sip_file_body_handler_recv_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, const uint8_t *buf, size_t size) {
	FILE *f;
	int ret;
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	if (obj->filepath == NULL) return;
	f = fopen(obj->filepath, "ab");
	if (f == NULL) return;
	ret = fwrite(buf, 1, size, f);
	if (ret != size) {
		fclose(f);
		return;
	}
	fclose(f);
}

static int belle_sip_file_body_handler_send_chunk(belle_sip_body_handler_t *base, belle_sip_message_t *msg, size_t offset, uint8_t *buf, size_t *size) {
	FILE *f;
	int ret;
	belle_sip_file_body_handler_t *obj = (belle_sip_file_body_handler_t *)base;
	size_t to_send = MIN(*size, obj->base.expected_size - offset);
	if (obj->filepath == NULL) return BELLE_SIP_STOP;
	f = fopen(obj->filepath, "rb");
	if (f == NULL) return BELLE_SIP_STOP;
	ret = fseek(f, offset, SEEK_SET);
	if (ret < 0) {
		fclose(f);
		return BELLE_SIP_STOP;
	}
	ret = fread(buf, 1, to_send, f);
	if (ret < 0) {
		fclose(f);
		return BELLE_SIP_STOP;
	}
	*size = ret;
	fclose(f);
	return (((obj->base.expected_size - offset) == *size) || (*size == 0)) ? BELLE_SIP_STOP : BELLE_SIP_CONTINUE;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_file_body_handler_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_file_body_handler_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_file_body_handler_t,belle_sip_body_handler_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_file_body_handler_destroy,
			(belle_sip_object_clone_t)belle_sip_file_body_handler_clone,
			NULL
		},
		belle_sip_file_body_handler_recv_chunk,
		belle_sip_file_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_file_body_handler_t *belle_sip_file_body_handler_new(const char *filepath, belle_sip_body_handler_progress_callback_t progress_cb, void *data) {
	struct stat statbuf;
	belle_sip_file_body_handler_t *obj = belle_sip_object_new(belle_sip_file_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj, progress_cb, data);
	obj->filepath = belle_sip_strdup(filepath);
	if (stat(obj->filepath, &statbuf) == 0) {
		obj->base.expected_size = statbuf.st_size;
	}
	return obj;
}


/*
 * Multipart body handler implementation
 * TODO
**/

#define MULTIPART_SEPARATOR "--" BELLESIP_MULTIPART_BOUNDARY "\r\n"
#define MULTIPART_END "\r\n--" BELLESIP_MULTIPART_BOUNDARY "--\r\n"
struct belle_sip_multipart_body_handler{
	belle_sip_body_handler_t base;
	belle_sip_list_t *parts;
};

static void belle_sip_multipart_body_handler_destroy(belle_sip_multipart_body_handler_t *obj){
	belle_sip_list_free_with_data(obj->parts,belle_sip_object_unref);
}

static void belle_sip_multipart_body_handler_clone(belle_sip_multipart_body_handler_t *obj){
	obj->parts=belle_sip_list_copy_with_data(obj->parts,(void *(*)(void*))belle_sip_object_clone_and_ref);
}

static void belle_sip_multipart_body_handler_recv_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, size_t offset,
							const uint8_t *buffer, size_t size){
	/*TODO*/
}

static int belle_sip_multipart_body_handler_send_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, size_t offset,
							uint8_t *buffer, size_t *size){

	belle_sip_multipart_body_handler_t *obj_multipart=(belle_sip_multipart_body_handler_t*)obj;

	if (obj_multipart->parts->data) { /* we have a part, get its content from handler */
		int retval = BELLE_SIP_STOP;
		size_t offsetSize = 0; /* used to store size of data added by this function and not given by the body handler of current part */
		belle_sip_body_handler_t *current_part = (belle_sip_body_handler_t *)obj_multipart->parts->data;
		*size -= strlen(MULTIPART_END); /* just in case it will be the end of the message, ask for less characters than possible in order to be able to add the multipart message termination */

		if (current_part->transfered_size == 0) { /* Nothing transfered yet on this part, include a separator and the header if exists */
			size_t headersSize = 0;
			offsetSize = strlen(MULTIPART_SEPARATOR);

			if (current_part->headerStringBuffer != NULL) {
				headersSize = strlen(current_part->headerStringBuffer);
			}

			/* check if buffer is large enough to get the whole header + separtor and at least a byte of data */
			if (*size < headersSize+offsetSize+1) {
				return BELLE_SIP_BUFFER_OVERFLOW;
			}

			/* insert separator */
			memcpy(buffer, MULTIPART_SEPARATOR, offsetSize);
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
			if (obj_multipart->parts->next!=NULL) { /* there is an other part to be sent */
				obj_multipart->parts = belle_sip_list_next(obj_multipart->parts);
				return BELLE_SIP_CONTINUE;
			} else { /* there is nothing else, close the message and return STOP */
				memcpy(buffer+*size, MULTIPART_END, strlen(MULTIPART_END));
				*size+=strlen(MULTIPART_END);
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
			NULL
		},
		belle_sip_multipart_body_handler_recv_chunk,
		belle_sip_multipart_body_handler_send_chunk
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_multipart_body_handler_t *belle_sip_multipart_body_handler_new(belle_sip_body_handler_progress_callback_t progress_cb, void *data,
									 belle_sip_body_handler_t *first_part){
	belle_sip_multipart_body_handler_t *obj=belle_sip_object_new(belle_sip_multipart_body_handler_t);
	belle_sip_body_handler_init((belle_sip_body_handler_t*)obj,progress_cb,data);
	obj->base.expected_size = strlen(MULTIPART_END); /* body's length will be part length(including boundary) + multipart end */
	if (first_part) belle_sip_multipart_body_handler_add_part(obj,first_part);
	return obj;
}

#define DEFAULT_HEADER_STRING_SIZE 512
void belle_sip_multipart_body_handler_add_part(belle_sip_multipart_body_handler_t *obj, belle_sip_body_handler_t *part){
	obj->base.expected_size+=part->expected_size+strlen(MULTIPART_SEPARATOR); /* add the separator length to the body length as each part start with a separator */
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


