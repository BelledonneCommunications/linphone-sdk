/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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
#ifndef MSQUEUE_H
#define MSQUEUE_H

#include <ortp/str_utils.h>
#include <mediastreamer2/mscommon.h>


typedef struct _MSCPoint{
	struct _MSFilter *filter;
	int pin;
} MSCPoint;

typedef struct _MSQueue
{
	queue_t q;
	MSCPoint prev;
	MSCPoint next;
}MSQueue;


MS2_PUBLIC MSQueue * ms_queue_new(struct _MSFilter *f1, int pin1, struct _MSFilter *f2, int pin2 );

static MS2_INLINE mblk_t *ms_queue_get(MSQueue *q){
	return getq(&q->q);
}

static MS2_INLINE void ms_queue_put(MSQueue *q, mblk_t *m){
	putq(&q->q,m);
	return;
}

/**
 * Insert mblk_t 'm' in queue 'q' just before mblk_t 'em'.
 * If em is NULL, m is inserted at the end and becomes the last element.
 */
static MS2_INLINE void ms_queue_insert(MSQueue *q, mblk_t *em, mblk_t *m) {
    insq(&q->q, em, m);
    return;
}

static MS2_INLINE mblk_t * ms_queue_peek_last(MSQueue *q){
	return qlast(&q->q);
}

static MS2_INLINE mblk_t *ms_queue_peek_first(MSQueue *q){
	return qbegin(&q->q);
}

#define ms_queue_next(q, m) (m)->b_next

static MS2_INLINE bool_t ms_queue_end(MSQueue *q, mblk_t *m){
	return qend(&q->q,m);
}

static MS2_INLINE void ms_queue_remove(MSQueue *q, mblk_t *m){
	remq(&q->q,m);
}

static MS2_INLINE bool_t ms_queue_empty(MSQueue *q){
	return qempty(&q->q);
}

#ifdef __cplusplus
extern "C"
{
#endif

/*yes these functions need to be public for plugins to work*/

/*init a queue on stack*/
MS2_PUBLIC void ms_queue_init(MSQueue *q);

MS2_PUBLIC void ms_queue_flush(MSQueue *q);

MS2_PUBLIC void ms_queue_destroy(MSQueue *q);


#define __mblk_set_flag(m,pos,bitval) \
	(m)->reserved2=(m->reserved2 & ~(1<<pos)) | ((!!bitval)<<pos) 
	
#define mblk_set_timestamp_info(m,ts) (m)->reserved1=(ts);
#define mblk_get_timestamp_info(m)    ((m)->reserved1)
#define mblk_set_marker_info(m,bit)   __mblk_set_flag(m,0,bit)
#define mblk_get_marker_info(m)	      ((m)->reserved2 & 0x1) /*bit 1*/

#define mblk_set_precious_flag(m,bit)    __mblk_set_flag(m,1,bit)  /*use to prevent mirroring for video*/
#define mblk_get_precious_flag(m)    (((m)->reserved2)>>1 & 0x1) /*bit 2 */

#define mblk_set_plc_flag(m,bit)    __mblk_set_flag(m,2,bit)  /*use to mark a plc generated block*/
#define mblk_get_plc_flag(m)    (((m)->reserved2)>>2 & 0x1) /*bit 3*/

#define mblk_set_cng_flag(m,bit)    __mblk_set_flag(m,3,bit)  /*use to mark a cng generated block*/
#define mblk_get_cng_flag(m)    (((m)->reserved2)>>3 & 0x1) /*bit 4*/

#define mblk_set_user_flag(m,bit)    __mblk_set_flag(m,7,bit)  /* to be used by extensions to mediastreamer2*/
#define mblk_get_user_flag(m)    (((m)->reserved2)>>7 & 0x1) /*bit 8*/

#define mblk_set_cseq(m,value) (m)->reserved2=(m)->reserved2| ((value&0xFFFF)<<16);	
#define mblk_get_cseq(m) ((m)->reserved2>>16)

#define HAVE_ms_bufferizer_fill_current_metas
	
struct _MSBufferizer{
	queue_t q;
	size_t size;
};

typedef struct _MSBufferizer MSBufferizer;

/*allocates and initialize */
MS2_PUBLIC MSBufferizer * ms_bufferizer_new(void);

/*initialize in memory */
MS2_PUBLIC void ms_bufferizer_init(MSBufferizer *obj);

MS2_PUBLIC void ms_bufferizer_put(MSBufferizer *obj, mblk_t *m);

/* put every mblk_t from q, into the bufferizer */
MS2_PUBLIC void ms_bufferizer_put_from_queue(MSBufferizer *obj, MSQueue *q);

/*read bytes from bufferizer object*/
MS2_PUBLIC size_t ms_bufferizer_read(MSBufferizer *obj, uint8_t *data, size_t datalen);

/*obtain current meta-information of the last read bytes (if any) and copy them into 'm'*/
MS2_PUBLIC void ms_bufferizer_fill_current_metas(MSBufferizer *obj, mblk_t *m);

/* returns the number of bytes available in the bufferizer*/
static MS2_INLINE size_t ms_bufferizer_get_avail(MSBufferizer *obj){
	return obj->size;
}

MS2_PUBLIC void ms_bufferizer_skip_bytes(MSBufferizer *obj, int bytes);

/* purge all data pending in the bufferizer */
MS2_PUBLIC void ms_bufferizer_flush(MSBufferizer *obj);

MS2_PUBLIC void ms_bufferizer_uninit(MSBufferizer *obj);

MS2_PUBLIC void ms_bufferizer_destroy(MSBufferizer *obj);

#ifdef __cplusplus
}
#endif

#endif
