/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef msv4l_h
#define msv4l_h

#include <mediastreamer2/msfilter.h>

#define	MS_V4L_START		MS_FILTER_METHOD_NO_ARG(MS_V4L_ID,0)
#define	MS_V4L_STOP			MS_FILTER_METHOD_NO_ARG(MS_V4L_ID,1)
#define	MS_V4L_SET_DEVICE	MS_FILTER_METHOD(MS_V4L_ID,2,int)
#define	MS_V4L_SET_IMAGE	MS_FILTER_METHOD(MS_V4L_ID,3,char)

#endif
