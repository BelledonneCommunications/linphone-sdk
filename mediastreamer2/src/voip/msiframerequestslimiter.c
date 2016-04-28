#include "mediastreamer2/msiframerequestslimiter.h"

void ms_iframe_requests_limiter_init(MSIFrameRequestsLimiterCtx *obj, const MSTicker *t, int min_iframe_interval) {
	memset(obj, 0, sizeof(MSIFrameRequestsLimiterCtx));
	obj->ticker = t;
	obj->last_sent_iframe_time = t->time;
	obj->min_iframe_interval = min_iframe_interval;
}

void ms_iframe_requests_limiter_require_iframe(MSIFrameRequestsLimiterCtx *obj) {
	obj->iframe_required = TRUE;
}

bool_t ms_iframe_requests_limiter_iframe_sending_authorized(const MSIFrameRequestsLimiterCtx *obj) {
	return obj->ticker->time - obj->last_sent_iframe_time > obj->min_iframe_interval;
}

void ms_iframe_requests_limiter_notify_iframe_sent(MSIFrameRequestsLimiterCtx *obj) {
	obj->iframe_required = FALSE;
	obj->last_sent_iframe_time = obj->ticker->time;
}
