#include "mediastreamer2/msfilter.h"

extern MSFilterDesc ms_tee_desc;
extern MSFilterDesc ms_join_desc;
extern MSFilterDesc ms_void_sink_desc;
MSFilterDesc * ms_base_filter_descs[]={
&ms_tee_desc,
&ms_join_desc,
&ms_void_sink_desc,
NULL
};
