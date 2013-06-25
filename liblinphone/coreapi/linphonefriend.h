/*
linphonefriend.h
Copyright (C) 2010  Belledonne Communications SARL

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef LINPHONEFRIEND_H_
#define LINPHONEFRIEND_H_

#include "linphonepresence.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup buddy_list
 * @{
 */
/**
 * @ingroup buddy_list
 * Enum controlling behavior for incoming subscription request.
 * <br> Use by linphone_friend_set_inc_subscribe_policy()
 */
typedef  enum {
	/**
	 * Does not automatically accept an incoming subscription request.
	 * This policy implies that a decision has to be taken for each incoming subscription request notified by callback LinphoneCoreVTable.new_subscription_request
	 *
	 */
	LinphoneSPWait,
	/**
	 * Rejects incoming subscription request.
	 */
	LinphoneSPDeny,
	/**
	 * Automatically accepts a subscription request.
	 */
	LinphoneSPAccept
}LinphoneSubscribePolicy;

/**
 * Enum describing remote friend status
 * @deprecated Use #LinphonePresenceModel and #LinphonePresenceActivity instead
 */
typedef enum _LinphoneOnlineStatus{
	/**
	 * Offline
	 */
	LinphoneStatusOffline,
	/**
	 * Online
	 */
	LinphoneStatusOnline,
	/**
	 * Busy
	 */
	LinphoneStatusBusy,
	/**
	 * Be right back
	 */
	LinphoneStatusBeRightBack,
	/**
	 * Away
	 */
	LinphoneStatusAway,
	/**
	 * On the phone
	 */
	LinphoneStatusOnThePhone,
	/**
	 * Out to lunch
	 */
	LinphoneStatusOutToLunch,
	/**
	 * Do not disturb
	 */
	LinphoneStatusDoNotDisturb,
	/**
	 * Moved in this sate, call can be redirected if an alternate contact address has been set using function linphone_core_set_presence_info()
	 */
	LinphoneStatusMoved,
	/**
	 * Using another messaging service
	 */
	LinphoneStatusAltService,
	/**
	 * Pending
	 */
	LinphoneStatusPending,

        /**
         * Vacation
         */
        LinphoneStatusVacation,

	LinphoneStatusEnd
}LinphoneOnlineStatus;


struct _LinphoneFriend;
/**
 * Represents a buddy, all presence actions like subscription and status change notification are performed on this object
 */
typedef struct _LinphoneFriend LinphoneFriend;

/**
 * Contructor
 * @return a new empty #LinphoneFriend
 */
LinphoneFriend * linphone_friend_new();
/**
 * Contructor same as linphone_friend_new() + linphone_friend_set_addr()
 * @param addr a buddy address, must be a sip uri like sip:joe@sip.linphone.org
 * @return a new #LinphoneFriend with \link linphone_friend_get_address() address initialized \endlink
 */
LINPHONE_PUBLIC	LinphoneFriend *linphone_friend_new_with_addr(const char *addr);

/**
 * Destructor
 * @param lf #LinphoneFriend object
 */
void linphone_friend_destroy(LinphoneFriend *lf);

/**
 * set #LinphoneAddress for this friend
 * @param fr #LinphoneFriend object
 * @param address #LinphoneAddress
 */
int linphone_friend_set_addr(LinphoneFriend *fr, const LinphoneAddress* address);

/**
 * set the display name for this friend
 * @param lf #LinphoneFriend object
 * @param name 
 */
int linphone_friend_set_name(LinphoneFriend *lf, const char *name);

/**
 * get address of this friend
 * @param lf #LinphoneFriend object
 * @return #LinphoneAddress
 */
LINPHONE_PUBLIC	const LinphoneAddress *linphone_friend_get_address(const LinphoneFriend *lf);
/**
 * get subscription flag value
 * @param lf #LinphoneFriend object
 * @return returns true is subscription is activated for this friend
 *
 */
bool_t linphone_friend_subscribes_enabled(const LinphoneFriend *lf);
#define linphone_friend_get_send_subscribe linphone_friend_subscribes_enabled

/**
 * Configure #LinphoneFriend to subscribe to presence information
 * @param fr #LinphoneFriend object
 * @param val if TRUE this friend will receive subscription message
 */

LINPHONE_PUBLIC	int linphone_friend_enable_subscribes(LinphoneFriend *fr, bool_t val);

#define linphone_friend_send_subscribe linphone_friend_enable_subscribes
/**
 * Configure incoming subscription policy for this friend.
 * @param fr #LinphoneFriend object
 * @param pol #LinphoneSubscribePolicy policy to apply.
 */
int linphone_friend_set_inc_subscribe_policy(LinphoneFriend *fr, LinphoneSubscribePolicy pol);
/**
 * get current subscription policy for this #LinphoneFriend
 * @param lf #LinphoneFriend object
 * @return #LinphoneSubscribePolicy
 *
 */
LinphoneSubscribePolicy linphone_friend_get_inc_subscribe_policy(const LinphoneFriend *lf);

/**
 * Starts editing a friend configuration.
 *
 * Because friend configuration must be consistent, applications MUST
 * call linphone_friend_edit() before doing any attempts to modify
 * friend configuration (such as \link linphone_friend_set_addr() address \endlink  or \link linphone_friend_set_inc_subscribe_policy() subscription policy\endlink  and so on).
 * Once the modifications are done, then the application must call
 * linphone_friend_done() to commit the changes.
**/
LINPHONE_PUBLIC	void linphone_friend_edit(LinphoneFriend *fr);
/**
 * Commits modification made to the friend configuration.
 * @param fr #LinphoneFriend object
**/
LINPHONE_PUBLIC	void linphone_friend_done(LinphoneFriend *fr);




/**
 * @brief Get the status of a friend
 * @param[in] lf A #LinphoneFriend object
 * @return #LinphoneOnlineStatus
 * @deprecated Use linphone_friend_get_presence_model() instead
 */
LinphoneOnlineStatus linphone_friend_get_status(const LinphoneFriend *lf);

/**
 * @brief Get the presence information of a friend
 * @param[in] lf A #LinphoneFriend object
 * @return A #LinphonePresenceModel object, or NULL if the friend do not have presence information (in which case he is considered offline)
 */
const LinphonePresenceModel * linphone_friend_get_presence_model(LinphoneFriend *lf);

BuddyInfo * linphone_friend_get_info(const LinphoneFriend *lf);
void linphone_friend_set_ref_key(LinphoneFriend *lf, const char *key);
const char *linphone_friend_get_ref_key(const LinphoneFriend *lf);
bool_t linphone_friend_in_list(const LinphoneFriend *lf);

#define linphone_friend_url(lf) ((lf)->url)

/**
 * Return humain readable presence status
 * @param ss
 * @deprecated Use #LinphonePresenceModel, #LinphonePresenceActivity and linphone_presence_activity_to_string() instead.
 */
const char *linphone_online_status_to_string(LinphoneOnlineStatus ss);


/**
 * @brief Set my presence status
 * @param[in] lc #LinphoneCore object
 * @param[in] minutes_away how long in away
 * @param[in] alternative_contact sip uri used to redirect call in state #LinphoneStatusMoved
 * @param[in] os #LinphoneOnlineStatus
 * @deprecated Use linphone_core_set_presence_model() instead
 */
LINPHONE_PUBLIC void linphone_core_set_presence_info(LinphoneCore *lc,int minutes_away,const char *alternative_contact,LinphoneOnlineStatus os);

/**
 * @brief Set my presence status
 * @param[in] lc #LinphoneCore object
 * @param[in] presence #LinphonePresenceModel
 */
LINPHONE_PUBLIC void linphone_core_set_presence_model(LinphoneCore *lc, LinphonePresenceModel *presence);

/**
 * @brief Get my presence status
 * @param[in] lc #LinphoneCore object
 * @return #LinphoneOnlineStatus
 * @deprecated Use linphone_core_get_presence_model() instead
 */
LinphoneOnlineStatus linphone_core_get_presence_info(const LinphoneCore *lc);

/**
 * @brief Get my presence status
 * @param[in] lc #LinphoneCore object
 * @return A #LinphonePresenceModel object, or NULL if no presence model has been set.
 */
LinphonePresenceModel * linphone_core_get_presence_model(const LinphoneCore *lc);

void linphone_core_interpret_friend_uri(LinphoneCore *lc, const char *uri, char **result);
/**
 * Add a friend to the current buddy list, if \link linphone_friend_enable_subscribes() subscription attribute \endlink is set, a SIP SUBSCRIBE message is sent.
 * @param lc #LinphoneCore object
 * @param fr #LinphoneFriend to add
 */
LINPHONE_PUBLIC	void linphone_core_add_friend(LinphoneCore *lc, LinphoneFriend *fr);
/**
 * remove a friend from the buddy list
 * @param lc #LinphoneCore object
 * @param fr #LinphoneFriend to add
 */
void linphone_core_remove_friend(LinphoneCore *lc, LinphoneFriend *fr);
/**
 * Black list a friend. same as linphone_friend_set_inc_subscribe_policy() with #LinphoneSPDeny policy;
 * @param lc #LinphoneCore object
 * @param lf #LinphoneFriend to add
 */
void linphone_core_reject_subscriber(LinphoneCore *lc, LinphoneFriend *lf);
/**
 * get Buddy list of LinphoneFriend
 * @param lc #LinphoneCore object
 * */
LINPHONE_PUBLIC	const MSList * linphone_core_get_friend_list(const LinphoneCore *lc);
/**
 *  notify all friends that have subscribed
 * @param lc #LinphoneCore object
 * @param os #LinphoneOnlineStatus to notify
 *  */
void linphone_core_notify_all_friends(LinphoneCore *lc, LinphonePresenceModel *presence);
LinphoneFriend *linphone_core_get_friend_by_address(const LinphoneCore *lc, const char *addr);
LinphoneFriend *linphone_core_get_friend_by_ref_key(const LinphoneCore *lc, const char *key);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* LINPHONEFRIEND_H_ */
