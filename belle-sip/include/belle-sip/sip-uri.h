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

#ifndef BELLE_SIP_URI_H_
#define BELLE_SIP_URI_H_

#include "belle-sip/defs.h"
#include "belle-sip/list.h"
#include "belle-sip/utils.h"
#include "belle-sip/types.h"

BELLE_SIP_BEGIN_DECLS

/**
 *
 */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_uri_new(void);

/**
 *
 */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_uri_parse (const char* uri) ;
/**
 *
 */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_uri_create (const char* username,const char* host) ;


/**
 *	Returns the value of the named header, or null if it is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_header(const belle_sip_uri_t* uri,const char* name);


/**
 *	remove all headers
 *
 */
BELLESIP_EXPORT void belle_sip_uri_headers_clean(belle_sip_uri_t* uri);


/**
 * Returns an Iterator over the const char*names of all headers present in this SipURI.
 *
 */
BELLESIP_EXPORT const belle_sip_list_t*	belle_sip_uri_get_header_names(const belle_sip_uri_t* uri) ;
/**
 * 	          Returns the host part of this SipURI.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_host(const belle_sip_uri_t* uri) ;
/**
 * 	          Returns the value of the maddr parameter, or null if this is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_maddr_param(const belle_sip_uri_t* uri) ;
/**
 *	          Returns the value of the method parameter, or null if this is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_method_param(const belle_sip_uri_t* uri) ;
/**
 *	          Returns the port part of this SipURI.
 *
 */
BELLESIP_EXPORT int	belle_sip_uri_get_port(const belle_sip_uri_t* uri) ;
/**
 * Returns the port of the uri, if not specified in the uri returns the well known port according to the transport.
**/
BELLESIP_EXPORT int belle_sip_uri_get_listening_port(const belle_sip_uri_t *uri);
/**
 * 	          Returns the value of the "transport" parameter, or null if this is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_transport_param(const belle_sip_uri_t* uri) ;
/**
 * 	          Returns the value of the "ttl" parameter, or -1 if this is not set.
 *
 */
BELLESIP_EXPORT int	belle_sip_uri_get_ttl_param(const belle_sip_uri_t* uri) ;
/**
 * 	          Returns the user part of this SipURI.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_user(const belle_sip_uri_t* uri) ;
/**
 * 	          Returns the value of the userParam, or null if this is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_user_param(const belle_sip_uri_t* uri) ;
/**
 * 	          Gets user password of SipURI, or null if it is not set.
 *
 */
BELLESIP_EXPORT const char*	belle_sip_uri_get_user_password(const belle_sip_uri_t* uri) ;
/**
 *	          Returns whether the the lr parameter is set.
 *
 */
BELLESIP_EXPORT unsigned int	belle_sip_uri_has_lr_param(const belle_sip_uri_t* uri) ;
/**
 *
 * 	          Returns true if this SipURI is secure i.e. if this SipURI represents a sips URI.
 *
 */
BELLESIP_EXPORT unsigned int	belle_sip_uri_is_secure(const belle_sip_uri_t* uri) ;

/**
 * 	          Sets the value of the specified header fields to be included in a request constructed from the URI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_header(belle_sip_uri_t* uri,const char*name, const char*value) ;

/**
 * Removes specified header from uri.
**/
BELLESIP_EXPORT void belle_sip_uri_remove_header(belle_sip_uri_t *uri, const char *name);

/**
 * 	          Removes the port part of this SipURI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_remove_port(belle_sip_uri_t* uri) ;
/**
 * 	          Set the host part of this SipURI to the newly supplied host parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_host(belle_sip_uri_t* uri,const char*host) ;
/**
 * 	          Sets the value of the lr parameter of this SipURI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_lr_param(belle_sip_uri_t* uri,unsigned int param) ;
/**
 *          Sets the value of the maddr parameter of this SipURI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_maddr_param(belle_sip_uri_t* uri,const char*mAddr) ;
/**
 *	          Sets the value of the method parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_method_param(belle_sip_uri_t* uri,const char*method) ;
/**
 * 	          Set the port part of this SipURI to the newly supplied port parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_port(belle_sip_uri_t* uri, int port) ;
/**
 * 	          Sets the scheme of this URI to sip or sips depending on whether the argument is true or false.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_secure(belle_sip_uri_t* uri,unsigned int secure) ;
/**
 * 	          Sets the value of the "transport" parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_transport_param(belle_sip_uri_t* uri,const char*transport) ;
/**
 *  	          Sets the value of the ttl parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_ttl_param(belle_sip_uri_t* uri,int ttl) ;
/**
 *  	          Sets the user of SipURI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_user(belle_sip_uri_t* uri,const char*user) ;
/**
 * 	          Sets the value of the user parameter.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_user_param(belle_sip_uri_t* uri,const char*userParam) ;
/**
 * 	          Sets the user password associated with the user of SipURI.
 *
 */
BELLESIP_EXPORT void	belle_sip_uri_set_user_password(belle_sip_uri_t* uri,const char*userPassword) ;

/**
 * 	          This method returns the URI as a string.
 *
 */
BELLESIP_EXPORT char*	belle_sip_uri_to_string(const belle_sip_uri_t* uri) ;

belle_sip_error_code belle_sip_uri_marshal(const belle_sip_uri_t* uri, char* buff, size_t buff_size, size_t *offset);

#define BELLE_SIP_URI(obj) BELLE_SIP_CAST(obj,belle_sip_uri_t)

/**define URI equality as using comparison rules from RFC3261 section 19.1.4
 * @param belle_sip_uri_t* uri_a
 * @param belle_sip_uri_t* uri_a
 * @return 0 if not matched.
 *
 * */
BELLESIP_EXPORT int belle_sip_uri_equals(const belle_sip_uri_t* uri_a,const belle_sip_uri_t* uri_b);
/**
 * returns 0 if uri does follows components requirement for being a request uri
 * */
BELLESIP_EXPORT int belle_sip_uri_check_components_from_request_uri(const belle_sip_uri_t* uri);
/**
 * returns 0 if uri does follows components requirement for a given method/header
 */
BELLESIP_EXPORT int belle_sip_uri_check_components_from_context(const belle_sip_uri_t* uri,const char* method,const char* header_name);
BELLE_SIP_END_DECLS
#ifndef BELLE_SIP_USE_STL
#define BELLE_SIP_USE_STL 1
#endif

#if __cplusplus >= 201103L && BELLE_SIP_USE_STL
#include <ostream>
inline   std::ostream&
operator<<( std::ostream& __os, const belle_sip_uri_t* uri)
{
	char* uri_as_string = belle_sip_uri_to_string(uri);
	__os << uri_as_string;
	belle_sip_free(uri_as_string);
	return __os;
}
namespace std {
	template <> struct hash<const belle_sip_uri_t*> {
		size_t operator()(const belle_sip_uri_t *x ) const {
			hash<string> H;
			size_t h=0;
			if (belle_sip_uri_get_user(x))
				h = H(belle_sip_uri_get_user(x));
			if (belle_sip_uri_get_host(x))
				h ^=H(belle_sip_uri_get_host(x));
			if (belle_sip_uri_get_port(x)>0) {
				std::hash<int> H2;
				h ^=H2(belle_sip_uri_get_port(x));
			}
			if (belle_sip_uri_get_transport_param(x)) {
				h ^=H(belle_sip_uri_get_transport_param(x));
			}
			if (belle_sip_uri_is_secure(x))
				h+=1;
	
			return h;
		}
	};
}

#include <functional>

namespace bellesip {

struct UriComparator : public std::binary_function<belle_sip_uri_t*, belle_sip_uri_t*, bool> {
	bool operator()(const belle_sip_uri_t* lhs, const belle_sip_uri_t* rhs) const {
		return belle_sip_uri_equals(lhs,rhs);
	}
};
}



#endif

#endif  /*BELLE_SIP_URI_H_*/

