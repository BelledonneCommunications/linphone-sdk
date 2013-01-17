#ifndef MS_VIDEO_NEON_H
#define MS_VIDEO_NEON_H

void rotate_down_scale_plane_neon_anticlockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst,bool_t down_scale);

void rotate_down_scale_plane_neon_anticlockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst,bool_t down_scale);

void rotate_down_scale_cbcr_to_cr_cb(int wDest, int hDest, int full_width, uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise,bool_t down_scale);

void rotate_down_scale_plane_neon_clockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst,bool_t down_scale);

#endif

