/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"

int main(int argc, char *argv[]) {

	MSFactory *factory = NULL;
	factory = ms_factory_new_with_voip();

	if (argc < 2) {
		ms_error("Usage: mtudiscover [host]");
		return -1;
	}
	bctbx_set_log_level_mask(BCTBX_LOG_DOMAIN,
	                         BCTBX_LOG_MESSAGE | BCTBX_LOG_WARNING | BCTBX_LOG_ERROR | BCTBX_LOG_FATAL);
	printf("result: %i \n", ms_discover_mtu(argv[1]));

	ms_factory_destroy(factory);

	return 0;
}
