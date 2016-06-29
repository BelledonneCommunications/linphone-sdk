/*
 *  BCUnit - A Unit testing framework library for C.
 *  Copyright (C) 2006  Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Internationalization support
 *
 *  05-May-2006   Initial implementation.  (JDS)
 */

/** @file
 *  Internal BCUnit header supporting internationalization of
 *  BCUnit framework & interfaces.
 */
/** @addtogroup Framework
 * @{
 */

#ifndef BCUNIT_BCUNIT_INTL_H_SEEN
#define BCUNIT_BCUNIT_INTL_H_SEEN

/* activate these when source preparation is complete
#include <libintl.h>
#ifndef _
#  define _(String) gettext (String)
#endif
#ifndef gettext_noop
#  define gettext_noop(String) String
#endif
#ifndef N_
#  define N_(String) gettext_noop (String)
#endif
*/

/* deactivate these when source preparation is complete */
#undef _
#define _(String) (String)
#undef N_
#define N_(String) String
#undef textdomain
#define textdomain(Domain)
#undef bindtextdomain
#define bindtextdomain(Package, Directory)

#endif  /*  BCUNIT_BCUNIT_INTL_H_SEEN  */

/** @} */
