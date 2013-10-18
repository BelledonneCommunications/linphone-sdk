/*
 * SipUri.h
 *
 *  Created on: 18 sept. 2010
 *      Author: jehanmonnier
 */

#ifndef BELLE_SIP_URI_H_
#define BELLE_SIP_URI_H_

#include "belle-sip/defs.h"
#include "belle-sip/list.h"
#include "belle-sip/utils.h"

/*inherite from belle_sip_parameters_t*/
typedef struct _belle_sip_uri belle_sip_uri_t;

BELLE_SIP_BEGIN_DECLS

/**
 *
 */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_uri_new();

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
BELLESIP_EXPORT char*	belle_sip_uri_to_string(belle_sip_uri_t* uri) ;

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



#endif  /*BELLE_SIP_URI_H_*/

