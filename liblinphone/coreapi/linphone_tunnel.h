/***************************************************************************
 *            linphone_tunnel.h
 *
 *  Fri Dec 9, 2011
 *  Copyright  2011  Belledonne Communications
 *  Author: Guillaume Beraudo
 *  Email: guillaume dot beraudo at linphone dot org
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef LINPHONETUNNEL_H
#define LINPHONETUNNEL_H

#include "linphonecore.h"

/**
 * @addtogroup tunnel
 * @{
**/

	/**
	 * This set of methods enhance  LinphoneCore functionalities in order to provide an easy to use API to
	 * - provision tunnel servers ip addresses and ports. This functionality is an option not implemented under GPL.
	 * - start/stop the tunneling service
	 * - perform auto-detection whether tunneling is required, based on a test of sending/receiving a flow of UDP packets.
	 *
	 * It takes in charge automatically the SIP registration procedure when connecting or disconnecting to a tunnel server.
	 * No other action on LinphoneCore is required to enable full operation in tunnel mode.
	**/

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _LinphoneTunnelConfig LinphoneTunnelConfig;

typedef enum _LinphoneTunnelMode {
	LinphoneTunnelModeDisabled,
	LinphoneTunnelModeEnabled,
	LinphoneTunnelModeAuto
} LinphoneTunnelMode;

/**
 * Create a new tunnel configuration
 */
LINPHONE_PUBLIC LinphoneTunnelConfig *linphone_tunnel_config_new(void);

/**
 * Set address of server.
 *
 * @param tunnel configuration object
 * @param host tunnel server ip address
 */
LINPHONE_PUBLIC void linphone_tunnel_config_set_host(LinphoneTunnelConfig *tunnel, const char *host);

/**
 * Get address of server.
 *
 * @param tunnel configuration object
 */
LINPHONE_PUBLIC const char *linphone_tunnel_config_get_host(const LinphoneTunnelConfig *tunnel);

/**
 * Set tls port of server.
 *
 * @param tunnel configuration object
 * @param port tunnel server tls port, recommended value is 443
 */
LINPHONE_PUBLIC void linphone_tunnel_config_set_port(LinphoneTunnelConfig *tunnel, int port);

/**
 * Get tls port of server.
 *
 * @param tunnel configuration object
 */
LINPHONE_PUBLIC int linphone_tunnel_config_get_port(const LinphoneTunnelConfig *tunnel);

/**
 * Set the remote port on the tunnel server side used to test udp reachability.
 *
 * @param tunnel configuration object
 * @param remote_udp_mirror_port remote port on the tunnel server side used to test udp reachability, set to -1 to disable the feature
 */
LINPHONE_PUBLIC void linphone_tunnel_config_set_remote_udp_mirror_port(LinphoneTunnelConfig *tunnel, int remote_udp_mirror_port);

/**
 * Get the remote port on the tunnel server side used to test udp reachability.
 *
 * @param tunnel configuration object
 */
LINPHONE_PUBLIC int linphone_tunnel_config_get_remote_udp_mirror_port(const LinphoneTunnelConfig *tunnel);

/**
 * Set the udp packet round trip delay in ms for a tunnel configuration.
 *
 * @param tunnel configuration object
 * @param delay udp packet round trip delay in ms considered as acceptable. recommended value is 1000 ms.
 */
LINPHONE_PUBLIC void linphone_tunnel_config_set_delay(LinphoneTunnelConfig *tunnel, int delay);

/**
 * Get the udp packet round trip delay in ms for a tunnel configuration.
 *
 * @param tunnel configuration object
 */
LINPHONE_PUBLIC int linphone_tunnel_config_get_delay(const LinphoneTunnelConfig *tunnel);

/**
 * Destroy a tunnel configuration
 *
 * @param tunnel configuration object
 */
LINPHONE_PUBLIC void linphone_tunnel_config_destroy(LinphoneTunnelConfig *tunnel);

/**
 * Add tunnel server configuration
 *
 * @param tunnel object
 * @param tunnel_config object
 */
LINPHONE_PUBLIC void linphone_tunnel_add_server(LinphoneTunnel *tunnel, LinphoneTunnelConfig *tunnel_config);

/**
 * Remove tunnel server configuration
 *
 * @param tunnel object
 * @param tunnel_config object
 */
LINPHONE_PUBLIC void linphone_tunnel_remove_server(LinphoneTunnel *tunnel, LinphoneTunnelConfig *tunnel_config);

/**
 * @param  tunnel object
 * returns a string of space separated list of host:port of tunnel server addresses
 * */
LINPHONE_PUBLIC const MSList *linphone_tunnel_get_servers(const LinphoneTunnel *tunnel);

/**
 * @param  tunnel object
 * Removes all tunnel server address previously entered with addServer()
**/
LINPHONE_PUBLIC void linphone_tunnel_clean_servers(LinphoneTunnel *tunnel);

/**
 * Sets whether tunneling of SIP and RTP is required.
 * @param  tunnel object
 * @param enabled If true enter in tunneled mode, if false exits from tunneled mode.
 * The TunnelManager takes care of refreshing SIP registration when switching on or off the tunneled mode.
 *
**/
LINPHONE_PUBLIC void linphone_tunnel_enable(LinphoneTunnel *tunnel, bool_t enabled);

/**
 * @param  tunnel object
 * Returns a boolean indicating whether tunneled operation is enabled.
**/
LINPHONE_PUBLIC bool_t linphone_tunnel_enabled(const LinphoneTunnel *tunnel);

/**
 * @param  tunnel object
 * Returns a boolean indicating whether tunnel is connected successfully.
**/
LINPHONE_PUBLIC bool_t linphone_tunnel_connected(const LinphoneTunnel *tunnel);

/**
 * @param  tunnel object
 * Forces reconnection to the tunnel server.
 * This method is useful when the device switches from wifi to Edge/3G or vice versa. In most cases the tunnel client socket
 * won't be notified promptly that its connection is now zombie, so it is recommended to call this method that will cause
 * the lost connection to be closed and new connection to be issued.
**/
LINPHONE_PUBLIC void linphone_tunnel_reconnect(LinphoneTunnel *tunnel);

/**
 * Start tunnel need detection.
 * @param  tunnel object
 * In auto detect mode, the tunnel manager try to establish a real time rtp cummunication with the tunnel server on  specified port.
 *<br>In case of success, the tunnel is automatically turned off. Otherwise, if no udp commmunication is feasible, tunnel mode is turned on.
 *<br> Call this method each time to run the auto detection algorithm
 */
LINPHONE_PUBLIC void linphone_tunnel_auto_detect(LinphoneTunnel *tunnel);

/**
 * Tells whether tunnel auto detection is enabled.
 * @param[in] tunnel LinphoneTunnel object.
 * @return TRUE if auto detection is enabled, FALSE otherwise.
 */
LINPHONE_PUBLIC bool_t linphone_tunnel_auto_detect_enabled(LinphoneTunnel *tunnel);

/**
 * @brief Set whether SIP packets must be directly sent to a UA or pass through the tunnel
 * @param tunnel Tunnel to configure
 * @param enable If true, SIP packets shall pass through the tunnel
 */
LINPHONE_PUBLIC void linphone_tunnel_enable_sip(LinphoneTunnel *tunnel, bool_t enable);

/**
 * @brief Check whether tunnel is set to transport SIP packets
 * @param tunnel Tunnel to check
 * @return True, SIP packets shall pass through through tunnel
 */
LINPHONE_PUBLIC bool_t linphone_tunnel_sip_enabled(const LinphoneTunnel *tunnel);

/**
 * Set an optional http proxy to go through when connecting to tunnel server.
 * @param tunnel LinphoneTunnel object
 * @param host Http proxy host.
 * @param port http proxy port.
 * @param username optional http proxy username if the proxy request authentication. Currently only basic authentication is supported. Use NULL if not needed.
 * @param passwd optional http proxy password. Use NULL if not needed.
 **/
LINPHONE_PUBLIC void linphone_tunnel_set_http_proxy(LinphoneTunnel *tunnel, const char *host, int port, const char* username,const char* passwd);

/**
 * Retrieve optional http proxy configuration previously set with linphone_tunnel_set_http_proxy().
 * @param tunnel LinphoneTunnel object
 * @param host Http proxy host.
 * @param port http proxy port.
 * @param username optional http proxy username if the proxy request authentication. Currently only basic authentication is supported. Use NULL if not needed.
 * @param passwd optional http proxy password. Use NULL if not needed.
 **/
LINPHONE_PUBLIC void linphone_tunnel_get_http_proxy(LinphoneTunnel*tunnel,const char **host, int *port, const char **username, const char **passwd);

LINPHONE_PUBLIC void linphone_tunnel_set_http_proxy_auth_info(LinphoneTunnel*tunnel, const char* username,const char* passwd);


/**
 * @}
**/

#ifdef __cplusplus
}
#endif


#endif //LINPHONETUNNEL_H

