/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL, Grenoble, France

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

#ifndef BELLE_GENERIC_URI_H_
#define BELLE_GENERIC_URI_H_


#include "belle-sip/defs.h"
#include "belle-sip/list.h"
#include "belle-sip/utils.h"



BELLE_SIP_BEGIN_DECLS

/**
 *
 */
BELLESIP_EXPORT belle_generic_uri_t* belle_generic_uri_new(void);

/**
 *
 */
BELLESIP_EXPORT belle_generic_uri_t* belle_generic_uri_parse (const char* uri);
/*
 * 	          Returns the host part of this uri.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_host(const belle_generic_uri_t* uri) ;
/**
 * 	          Returns the value of the maddr parameter, or null if this is not set.
 *
 */
BELLESIP_EXPORT int	belle_generic_uri_get_port(const belle_generic_uri_t* uri) ;
/**
 * Returns the port of the uri, if not specified in the uri returns the well known port according to the transport.
**/
BELLESIP_EXPORT int belle_generic_uri_get_listening_port(const belle_generic_uri_t *uri);

/**
 * 	          Returns the user part of this URI.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_user(const belle_generic_uri_t* uri) ;

/**
 * 	          Gets user password of uri, or null if it is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_user_password(const belle_generic_uri_t* uri) ;

/**
 *
 * 	          Returns uri scheme.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_scheme(const belle_generic_uri_t* uri) ;
/**
 *
 * 	          Returns uri path.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_path(const belle_generic_uri_t* uri) ;
/**
 *
 * 	          Returns uri query.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_query(const belle_generic_uri_t* uri) ;


/**
 * 	          Removes the port part of this uri.
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_remove_port(belle_generic_uri_t* uri) ;
/**
 * 	          Set the host part of this uri to the newly supplied host parameter.
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_host(belle_generic_uri_t* uri,const char*host) ;

/**
 * 	          Set the port part of this uri to the newly supplied port parameter.
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_port(belle_generic_uri_t* uri, int port) ;
/**
 * 	          Sets the scheme of this URI .
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_scheme(belle_generic_uri_t* uri,const char* scheme) ;
/**
 * 	          Sets the path of this URI .
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_path(belle_generic_uri_t* uri,const char* scheme) ;
/**
 * 	          Sets the query of this URI .
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_query(belle_generic_uri_t* uri,const char* scheme) ;

/**
 *  	          Sets the user of uri.
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_user(belle_generic_uri_t* uri,const char*user) ;

/**
 * 	          Sets the user password associated with the user of uri.
 *
 */
BELLESIP_EXPORT void	belle_generic_uri_set_user_password(belle_generic_uri_t* uri,const char*userPassword) ;

/**
 * 	          This method returns the URI as a string.
 *
 */
BELLESIP_EXPORT char*	belle_generic_uri_to_string(belle_generic_uri_t* uri) ;

BELLESIP_EXPORT belle_sip_error_code belle_generic_uri_marshal(const belle_generic_uri_t* uri, char* buff, size_t buff_size, size_t *offset);


/**
 * 	         gets opaque part of this uri if hierarchies part not detected.
 *
 */
BELLESIP_EXPORT const char*	belle_generic_uri_get_opaque_part(const belle_generic_uri_t* uri) ;

/**
 * 	         sets opaque part of this uri. Means hierarchies part is ignored if present.
 *
 */
BELLESIP_EXPORT void belle_generic_uri_set_opaque_part(belle_generic_uri_t* uri,const char * opaque_part) ;


#define BELLE_GENERIC_URI(obj) BELLE_SIP_CAST(obj,belle_generic_uri_t)




BELLE_SIP_END_DECLS




#endif /* belle_generic_uri_H_ */
