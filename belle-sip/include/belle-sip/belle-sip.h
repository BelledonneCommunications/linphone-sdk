/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

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
#ifndef BELLE_SIP_H
#define BELLE_SIP_H

#include "belle-sip/types.h"
#include "belle-sip/utils.h"
#include "belle-sip/list.h"
#include "belle-sip/listener.h"
#include "belle-sip/mainloop.h"
#include "belle-sip/sip-uri.h"
#include "belle-sip/headers.h"
#include "belle-sip/parameters.h"
#include "belle-sip/message.h"
#include "belle-sip/refresher.h"
#include "belle-sip/transaction.h"
#include "belle-sip/dialog.h"
#include "belle-sip/sipstack.h"
#include "belle-sip/resolver.h"
#include "belle-sip/listeningpoint.h"
#include "belle-sip/provider.h"
#include "belle-sip/auth-helper.h"
#include "belle-sip/generic-uri.h"
#include "belle-sip/http-listener.h"
#include "belle-sip/http-provider.h"
#include "belle-sip/http-listener.h"
#include "belle-sip/http-message.h"
#include "belle-sip/belle-sdp.h"
#include "belle-sip/bodyhandler.h"

#ifdef ANDROID
#include "belle-sip/wakelock.h"
#endif


#define BELLE_SIP_POINTER_TO_INT(p)	((int)(intptr_t)(p))
#define BELLE_SIP_INT_TO_POINTER(i)	((void*)(intptr_t)(i))

#endif

