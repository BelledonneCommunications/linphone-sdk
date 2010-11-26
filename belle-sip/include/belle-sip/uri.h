/*
 * SipUri.h
 *
 *  Created on: 18 sept. 2010
 *      Author: jehanmonnier
 */

#ifndef BELLE_SIP_URI_H_
#define BELLE_SIP_URI_H_


#include "belle-sip/list.h"

typedef struct _belle_sip_uri belle_sip_uri_t;

	/**
	 *
	 */
	belle_sip_uri_t* belle_sip_uri_new();
	/**
	 *
	 */
	void belle_sip_uri_delete(belle_sip_uri_t* uri);
	/**
	 *
	 */
	belle_sip_uri_t* belle_sip_uri_parse (const char* uri) ;
	/**
	 *
	 */
	belle_sip_uri_t* belle_sip_uri_ref (belle_sip_uri_t* address) ;
	/**
	 *
	 */
	void belle_sip_uri_unref (belle_sip_uri_t* address) ;
	/**
	 *	Returns the value of the named header, or null if it is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_header(belle_sip_uri_t* uri,const char* name);
			/**
			 * Returns an Iterator over the const char*names of all headers present in this SipURI.
			 *
			 */
	 belle_sip_list_t*	belle_sip_uri_get_header_names(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the host part of this SipURI.
	 *
	 */
	 const char*	belle_sip_uri_get_host(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the value of the maddr parameter, or null if this is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_maddr_param(belle_sip_uri_t* uri) ;
	/**
	 *	          Returns the value of the method parameter, or null if this is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_method_param(belle_sip_uri_t* uri) ;
	/**
	 *	          Returns the port part of this SipURI.
	 *
	 */
	 unsigned int	belle_sip_uri_get_port(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the value of the "transport" parameter, or null if this is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_transport_param(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the value of the "ttl" parameter, or -1 if this is not set.
	 *
	 */
	 int	belle_sip_uri_get_ttl_param(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the user part of this SipURI.
	 *
	 */
	 const char*	belle_sip_uri_get_user(belle_sip_uri_t* uri) ;
	/**
	 * 	          Returns the value of the userParam, or null if this is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_user_param(belle_sip_uri_t* uri) ;
	/**
	 * 	          Gets user password of SipURI, or null if it is not set.
	 *
	 */
	 const char*	belle_sip_uri_get_user_password(belle_sip_uri_t* uri) ;
	/**
	 *	          Returns whether the the lr parameter is set.
	 *
	 */
	 unsigned int	belle_sip_uri_has_lr_param(belle_sip_uri_t* uri) ;
	/**
	 *
	 * 	          Returns true if this SipURI is secure i.e. if this SipURI represents a sips URI.
	 *
	 */
	 unsigned int	belle_sip_uri_is_secure(belle_sip_uri_t* uri) ;
	/**
	 * 	          Removes the port part of this SipURI.
	 *
	 */
	 void	belle_sip_uri_remove_port(belle_sip_uri_t* uri) ;
	/**
	 * 	          Sets the value of the specified header fields to be included in a request constructed from the URI.
	 *
	 */
	 void	belle_sip_uri_set_header(belle_sip_uri_t* uri,const char*name, const char*value) ;
	/**
	 * 	          Set the host part of this SipURI to the newly supplied host parameter.
	 *
	 */
	 void	belle_sip_uri_set_host(belle_sip_uri_t* uri,const char*host) ;
	/**
	 * 	          Sets the value of the lr parameter of this SipURI.
	 *
	 */
	 void	belle_sip_uri_set_lr_param(belle_sip_uri_t* uri,unsigned int param) ;
	/**
	 *          Sets the value of the maddr parameter of this SipURI.
	 *
	 */
	 void	belle_sip_uri_set_maddr_param(belle_sip_uri_t* uri,const char*mAddr) ;
	/**
	 *	          Sets the value of the method parameter.
	 *
	 */
	 void	belle_sip_uri_set_method_param(belle_sip_uri_t* uri,const char*method) ;
	/**
	 * 	          Set the port part of this SipURI to the newly supplied port parameter.
	 *
	 */
	 void	belle_sip_uri_set_port(belle_sip_uri_t* uri,unsigned int port) ;
	/**
	 * 	          Sets the scheme of this URI to sip or sips depending on whether the argument is true or false.
	 *
	 */
	 void	belle_sip_uri_set_secure(belle_sip_uri_t* uri,unsigned int secure) ;
	/**
	 * 	          Sets the value of the "transport" parameter.
	 *
	 */
	 void	belle_sip_uri_set_transport_param(belle_sip_uri_t* uri,const char*transport) ;
	/**
	 *  	          Sets the value of the ttl parameter.
	 *
	 */
	 void	belle_sip_uri_set_ttl_param(belle_sip_uri_t* uri,int ttl) ;
	/**
	 *  	          Sets the user of SipURI.
	 *
	 */
	 void	belle_sip_uri_set_user(belle_sip_uri_t* uri,const char*user) ;
	/**
	 * 	          Sets the value of the user parameter.
	 *
	 */
	 void	belle_sip_uri_set_user_param(belle_sip_uri_t* uri,const char*userParam) ;
	/**
	 * 	          Sets the user password associated with the user of SipURI.
	 *
	 */
	 void	belle_sip_uri_set_user_password(belle_sip_uri_t* uri,const char*userPassword) ;
	/**
	 * 	          This method returns the URI as a string.
	 *
	 */
	 char*	belle_sip_uri_to_string(belle_sip_uri_t* uri) ;


#endif  /*BELLE_SIP_URI_H_*/

