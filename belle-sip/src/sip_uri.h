/*
 * SipUri.h
 *
 *  Created on: 18 sept. 2010
 *      Author: jehanmonnier
 */

#ifndef SIPURI_H_
#define SIPURI_H_



typedef struct _sip_uri sip_uri;

	sip_uri* sip_uri_new();

	void sip_uri_delete(sip_uri* uri);
	sip_uri* sip_uri_parse (const char* uri) ;

	/**
	 *	Returns the value of the named header, or null if it is not set.
	 *
	 */
	 const char*	sip_uri_get_header(sip_uri* uri,const char* name);
			/**
			 * Returns an Iterator over the const char*names of all headers present in this SipURI.
			 *
			 */
	 /*list<const char*>::iterator	sip_uri_get_header_names(SipUri* uri) ;*/
	/**
	 * 	          Returns the host part of this SipURI.
	 *
	 */
	 const char*	sip_uri_get_host(sip_uri* uri) ;
	/**
	 * 	          Returns the value of the maddr parameter, or null if this is not set.
	 *
	 */
	 const char*	sip_uri_get_maddr_param(sip_uri* uri) ;
	/**
	 *	          Returns the value of the method parameter, or null if this is not set.
	 *
	 */
	 const char*	sip_uri_get_method_param(sip_uri* uri) ;
	/**
	 *	          Returns the port part of this SipURI.
	 *
	 */
	 unsigned short	sip_uri_get_port(sip_uri* uri) ;
	/**
	 * 	          Returns the value of the "transport" parameter, or null if this is not set.
	 *
	 */
	 const char*	sip_uri_get_transport_param(sip_uri* uri) ;
	/**
	 * 	          Returns the value of the "ttl" parameter, or -1 if this is not set.
	 *
	 */
	 int	sip_uri_get_ttl_param(sip_uri* uri) ;
	/**
	 * 	          Returns the user part of this SipURI.
	 *
	 */
	 const char*	sip_uri_get_user(sip_uri* uri) ;
	/**
	 * 	          Returns the value of the userParam, or null if this is not set.
	 *
	 */
	 const char*	sip_uri_get_user_param(sip_uri* uri) ;
	/**
	 * 	          Gets user password of SipURI, or null if it is not set.
	 *
	 */
	 const char*	sip_uri_get_user_password(sip_uri* uri) ;
	/**
	 *	          Returns whether the the lr parameter is set.
	 *
	 */
	 unsigned int	sip_uri_has_lr_param(sip_uri* uri) ;
	/**
	 *
	 * 	          Returns true if this SipURI is secure i.e. if this SipURI represents a sips URI.
	 *
	 */
	 unsigned int	sip_uri_is_secure(sip_uri* uri) ;
	/**
	 * 	          Removes the port part of this SipURI.
	 *
	 */
	 void	sip_uri_remove_port(sip_uri* uri) ;
	/**
	 * 	          Sets the value of the specified header fields to be included in a request constructed from the URI.
	 *
	 */
	 void	sip_uri_set_header(sip_uri* uri,const char*name, const char*value) ;
	/**
	 * 	          Set the host part of this SipURI to the newly supplied host parameter.
	 *
	 */
	 void	sip_uri_set_host(sip_uri* uri,const char*host) ;
	/**
	 * 	          Sets the value of the lr parameter of this SipURI.
	 *
	 */
	 void	sip_uri_set_lr_param(sip_uri* uri) ;
	/**
	 *          Sets the value of the maddr parameter of this SipURI.
	 *
	 */
	 void	sip_uri_set_maddr_param(sip_uri* uri,const char*mAddr) ;
	/**
	 *	          Sets the value of the method parameter.
	 *
	 */
	 void	sip_uri_set_method_param(sip_uri* uri,const char*method) ;
	/**
	 * 	          Set the port part of this SipURI to the newly supplied port parameter.
	 *
	 */
	 void	sip_uri_set_port(sip_uri* uri,unsigned short port) ;
	/**
	 * 	          Sets the scheme of this URI to sip or sips depending on whether the argument is true or false.
	 *
	 */
	 void	sip_uri_set_secure(sip_uri* uri,unsigned secure) ;
	/**
	 * 	          Sets the value of the "transport" parameter.
	 *
	 */
	 void	sip_uri_set_transport_param(sip_uri* uri,const char*transport) ;
	/**
	 *  	          Sets the value of the ttl parameter.
	 *
	 */
	 void	sip_uri_set_ttl_param(sip_uri* uri,int ttl) ;
	/**
	 *  	          Sets the user of SipURI.
	 *
	 */
	 void	sip_uri_set_user(sip_uri* uri,const char*user) ;
	/**
	 * 	          Sets the value of the user parameter.
	 *
	 */
	 void	sip_uri_set_user_param(sip_uri* uri,const char*userParam) ;
	/**
	 * 	          Sets the user password associated with the user of SipURI.
	 *
	 */
	 void	sip_uri_set_user_password(sip_uri* uri,const char*userPassword) ;
	/**
	 * 	          This method returns the URI as a string.
	 *
	 */
	 char*	sip_uri_to_string(sip_uri* uri) ;


#endif  /*SIPURI_H_*/

