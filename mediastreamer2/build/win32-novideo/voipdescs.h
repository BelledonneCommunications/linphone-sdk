#include "mediastreamer2/msfilter.h"

extern MSFilterDesc ms_alaw_dec_desc;
extern MSFilterDesc ms_alaw_enc_desc;
extern MSFilterDesc ms_ulaw_dec_desc;
extern MSFilterDesc ms_ulaw_enc_desc;
extern MSFilterDesc ms_file_player_desc;
extern MSFilterDesc ms_rtp_send_desc;
extern MSFilterDesc ms_rtp_recv_desc;
extern MSFilterDesc ms_dtmf_gen_desc;
extern MSFilterDesc ms_file_rec_desc;
extern MSFilterDesc ms_speex_dec_desc;
extern MSFilterDesc ms_speex_enc_desc;
extern MSFilterDesc ms_gsm_dec_desc;
extern MSFilterDesc ms_gsm_enc_desc;
extern MSFilterDesc ms_speex_ec_desc;
extern MSFilterDesc ms_conf_desc;
//extern MSFilterDesc alsa_write_desc;
//extern MSFilterDesc alsa_read_desc;
//extern MSFilterDesc oss_read_desc;
//extern MSFilterDesc oss_write_desc;
//extern MSFilterDesc ms_arts_read_desc;
//extern MSFilterDesc ms_arts_write_desc;
//extern MSFilterDesc ms_v4l_desc;
//extern MSFilterDesc ms_sdl_out_desc;
//extern MSFilterDesc ms_h263_enc_desc;
//extern MSFilterDesc ms_h263_dec_desc;
extern MSFilterDesc ms_resample_desc;
extern MSFilterDesc ms_volume_desc;
extern MSFilterDesc ms_equalizer_desc;
MSFilterDesc * ms_voip_filter_descs[]={
&ms_alaw_dec_desc,
&ms_alaw_enc_desc,
&ms_ulaw_dec_desc,
&ms_ulaw_enc_desc,
&ms_file_player_desc,
&ms_rtp_send_desc,
&ms_rtp_recv_desc,
&ms_dtmf_gen_desc,
&ms_file_rec_desc,
&ms_speex_dec_desc,
&ms_speex_enc_desc,
&ms_gsm_dec_desc,
&ms_gsm_enc_desc,
&ms_speex_ec_desc,
&ms_conf_desc,
//&alsa_write_desc,
//&alsa_read_desc,
//&oss_read_desc,
//&oss_write_desc,
//&ms_arts_read_desc,
//&ms_arts_write_desc,
//&ms_v4l_desc,
//&ms_sdl_out_desc,
//&ms_h263_enc_desc,
//&ms_h263_dec_desc,
#ifndef DISABLE_RESAMPLE
&ms_resample_desc,
#endif
&ms_volume_desc,
&ms_equalizer_desc,
NULL
};
