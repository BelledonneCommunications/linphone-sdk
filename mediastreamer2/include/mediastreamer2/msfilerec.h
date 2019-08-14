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
#ifndef msfilerec_h
#define msfilerec_h

#include <mediastreamer2/msfilter.h>

extern MSFilterDesc ms_file_rec_desc;

#define MS_FILE_REC_OPEN	MS_FILTER_METHOD(MS_FILE_REC_ID,0,const char)
#define MS_FILE_REC_START	MS_FILTER_METHOD_NO_ARG(MS_FILE_REC_ID,1)
#define MS_FILE_REC_STOP	MS_FILTER_METHOD_NO_ARG(MS_FILE_REC_ID,2)
#define MS_FILE_REC_CLOSE	MS_FILTER_METHOD_NO_ARG(MS_FILE_REC_ID,3)



#endif
