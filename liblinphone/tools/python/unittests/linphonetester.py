from datetime import timedelta, datetime
from nose.tools import assert_equals
from copy import deepcopy
import linphone
import logging
import os
import time


test_username = "liblinphone_tester"
test_password = "secret"
test_route = "sip2.linphone.org"
tester_resources_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../tester/"))


def create_address(domain):
    addr = linphone.Address.new(None)
    assert addr != None
    addr.username = test_username
    assert_equals(addr.username, test_username)
    if domain is None:
        domain = test_route
    addr.domain = domain
    assert_equals(addr.domain, domain)
    addr.display_name = None
    addr.display_name = "Mr Tester"
    assert_equals(addr.display_name, "Mr Tester")
    return addr


class Logger(logging.Logger):

    def __init__(self, filename):
        logging.Logger.__init__(self, filename)
        handler = logging.FileHandler(filename)
        handler.setLevel(logging.INFO)
        formatter = logging.Formatter('%(asctime)s.%(msecs)03d %(levelname)s: %(message)s', '%H:%M:%S')
        handler.setFormatter(formatter)
        self.addHandler(handler)
        linphone.set_log_handler(self.log_handler)

    def log_handler(self, level, msg):
        method = getattr(self, level)
        if not msg.strip().startswith('[PYLINPHONE]'):
            msg = '[CORE] ' + msg
        method(msg)


class CoreManagerStats:
    def __init__(self):
        self.reset()

    def reset(self):
        self.number_of_LinphoneRegistrationNone = 0
        self.number_of_LinphoneRegistrationProgress = 0
        self.number_of_LinphoneRegistrationOk = 0
        self.number_of_LinphoneRegistrationCleared = 0
        self.number_of_LinphoneRegistrationFailed = 0
        self.number_of_auth_info_requested = 0

        self.number_of_LinphoneCallIncomingReceived = 0
        self.number_of_LinphoneCallOutgoingInit = 0
        self.number_of_LinphoneCallOutgoingProgress = 0
        self.number_of_LinphoneCallOutgoingRinging = 0
        self.number_of_LinphoneCallOutgoingEarlyMedia = 0
        self.number_of_LinphoneCallConnected = 0
        self.number_of_LinphoneCallStreamsRunning = 0
        self.number_of_LinphoneCallPausing = 0
        self.number_of_LinphoneCallPaused = 0
        self.number_of_LinphoneCallResuming = 0
        self.number_of_LinphoneCallRefered = 0
        self.number_of_LinphoneCallError = 0
        self.number_of_LinphoneCallEnd = 0
        self.number_of_LinphoneCallPausedByRemote = 0
        self.number_of_LinphoneCallUpdatedByRemote = 0
        self.number_of_LinphoneCallIncomingEarlyMedia = 0
        self.number_of_LinphoneCallUpdating = 0
        self.number_of_LinphoneCallReleased = 0

        self.number_of_LinphoneTransferCallOutgoingInit = 0
        self.number_of_LinphoneTransferCallOutgoingProgress = 0
        self.number_of_LinphoneTransferCallOutgoingRinging = 0
        self.number_of_LinphoneTransferCallOutgoingEarlyMedia = 0
        self.number_of_LinphoneTransferCallConnected = 0
        self.number_of_LinphoneTransferCallStreamsRunning = 0
        self.number_of_LinphoneTransferCallError = 0

        self.number_of_LinphoneMessageReceived = 0
        self.number_of_LinphoneMessageReceivedWithFile = 0
        self.number_of_LinphoneMessageReceivedLegacy = 0
        self.number_of_LinphoneMessageExtBodyReceived = 0
        self.number_of_LinphoneMessageInProgress = 0
        self.number_of_LinphoneMessageDelivered = 0
        self.number_of_LinphoneMessageNotDelivered = 0
        self.number_of_LinphoneIsComposingActiveReceived = 0
        self.number_of_LinphoneIsComposingIdleReceived = 0
        self.progress_of_LinphoneFileTransfer = 0

        self.number_of_IframeDecoded = 0

        self.number_of_NewSubscriptionRequest =0
        self.number_of_NotifyReceived = 0
        self.number_of_LinphonePresenceActivityOffline = 0
        self.number_of_LinphonePresenceActivityOnline = 0
        self.number_of_LinphonePresenceActivityAppointment = 0
        self.number_of_LinphonePresenceActivityAway = 0
        self.number_of_LinphonePresenceActivityBreakfast = 0
        self.number_of_LinphonePresenceActivityBusy = 0
        self.number_of_LinphonePresenceActivityDinner = 0
        self.number_of_LinphonePresenceActivityHoliday = 0
        self.number_of_LinphonePresenceActivityInTransit = 0
        self.number_of_LinphonePresenceActivityLookingForWork = 0
        self.number_of_LinphonePresenceActivityLunch = 0
        self.number_of_LinphonePresenceActivityMeal = 0
        self.number_of_LinphonePresenceActivityMeeting = 0
        self.number_of_LinphonePresenceActivityOnThePhone = 0
        self.number_of_LinphonePresenceActivityOther = 0
        self.number_of_LinphonePresenceActivityPerformance = 0
        self.number_of_LinphonePresenceActivityPermanentAbsence = 0
        self.number_of_LinphonePresenceActivityPlaying = 0
        self.number_of_LinphonePresenceActivityPresentation = 0
        self.number_of_LinphonePresenceActivityShopping = 0
        self.number_of_LinphonePresenceActivitySleeping = 0
        self.number_of_LinphonePresenceActivitySpectator = 0
        self.number_of_LinphonePresenceActivitySteering = 0
        self.number_of_LinphonePresenceActivityTravel = 0
        self.number_of_LinphonePresenceActivityTV = 0
        self.number_of_LinphonePresenceActivityUnknown = 0
        self.number_of_LinphonePresenceActivityVacation = 0
        self.number_of_LinphonePresenceActivityWorking = 0
        self.number_of_LinphonePresenceActivityWorship = 0

        self.number_of_inforeceived = 0

        self.number_of_LinphoneSubscriptionIncomingReceived = 0
        self.number_of_LinphoneSubscriptionOutgoingInit = 0
        self.number_of_LinphoneSubscriptionPending = 0
        self.number_of_LinphoneSubscriptionActive = 0
        self.number_of_LinphoneSubscriptionTerminated = 0
        self.number_of_LinphoneSubscriptionError = 0
        self.number_of_LinphoneSubscriptionExpiring = 0

        self.number_of_LinphonePublishProgress = 0
        self.number_of_LinphonePublishOk = 0
        self.number_of_LinphonePublishExpiring = 0
        self.number_of_LinphonePublishError = 0
        self.number_of_LinphonePublishCleared = 0

        self.number_of_LinphoneConfiguringSkipped = 0
        self.number_of_LinphoneConfiguringFailed = 0
        self.number_of_LinphoneConfiguringSuccessful = 0

        self.number_of_LinphoneCallEncryptedOn = 0
        self.number_of_LinphoneCallEncryptedOff = 0


class CoreManager:

    @classmethod
    def wait_for_until(cls, manager1, manager2, func, timeout):
        managers = []
        if manager1 is not None:
            managers.append(manager1)
        if manager2 is not None:
            managers.append(manager2)
        return cls.wait_for_list(managers, func, timeout)

    @classmethod
    def wait_for_list(cls, managers, func, timeout):
        start = datetime.now()
        end = start + timedelta(milliseconds = timeout)
        res = func(*managers)
        while not res and datetime.now() < end:
            for manager in managers:
                manager.lc.iterate()
            time.sleep(0.02)
            res = func(*managers)
        return res

    @classmethod
    def wait_for(cls, manager1, manager2, func):
        return cls.wait_for_until(manager1, manager2, func, 10000)

    @classmethod
    def call(cls, caller_manager, callee_manager, caller_params = None, callee_params = None, build_callee_params = False):
        initial_caller_stats = deepcopy(caller_manager.stats)
        initial_callee_stats = deepcopy(callee_manager.stats)

        # Use playfile for callee to avoid locking on capture card
        callee_manager.lc.use_files = True
        callee_manager.lc.play_file = os.path.join(tester_resources_path, 'sounds', 'hello8000.wav')

        if caller_params is None:
            call = caller_manager.lc.invite_address(callee_manager.identity)
        else:
            call = caller_manager.lc.invite_address_with_params(callee_manager.identity, caller_params)
        assert call is not None

        assert_equals(CoreManager.wait_for(callee_manager, caller_manager,
            lambda callee_manager, caller_manager: callee_manager.stats.number_of_LinphoneCallIncomingReceived == initial_callee_stats.number_of_LinphoneCallIncomingReceived + 1), True)
        assert_equals(callee_manager.lc.incoming_invite_pending, True)
        assert_equals(caller_manager.stats.number_of_LinphoneCallOutgoingProgress, initial_caller_stats.number_of_LinphoneCallOutgoingProgress + 1)

        retry = 0
        while (caller_manager.stats.number_of_LinphoneCallOutgoingRinging != initial_caller_stats.number_of_LinphoneCallOutgoingRinging + 1) and \
            (caller_manager.stats.number_of_LinphoneCallOutgoingEarlyMedia != initial_caller_stats.number_of_LinphoneCallOutgoingEarlyMedia + 1) and \
            retry < 20:
            retry += 1
            caller_manager.lc.iterate()
            callee_manager.lc.iterate()
            time.sleep(0.1)
        assert ((caller_manager.stats.number_of_LinphoneCallOutgoingRinging == initial_caller_stats.number_of_LinphoneCallOutgoingRinging + 1) or \
            (caller_manager.stats.number_of_LinphoneCallOutgoingEarlyMedia == initial_caller_stats.number_of_LinphoneCallOutgoingEarlyMedia + 1)) == True

        assert callee_manager.lc.current_call_remote_address is not None
        if caller_manager.lc.current_call is None or callee_manager.lc.current_call is None or callee_manager.lc.current_call_remote_address is None:
            return False
        callee_from_address = caller_manager.identity.clone()
        callee_from_address.port = 0 # Remove port because port is never present in from header
        assert_equals(callee_from_address.weak_equal(callee_manager.lc.current_call_remote_address), True)

        if callee_params is not None:
            callee_manager.lc.accept_call_with_params(callee_manager.lc.current_call, callee_params)
        elif build_callee_params:
            default_params = callee_manager.lc.create_call_params(callee_manager.lc.current_call)
            callee_manager.lc.accept_call_with_params(callee_manager.lc.current_call, default_params)
        else:
            callee_manager.lc.accept_call(callee_manager.lc.current_call)
        assert_equals(CoreManager.wait_for(callee_manager, caller_manager,
            lambda callee_manager, caller_manager: (callee_manager.stats.number_of_LinphoneCallConnected == initial_callee_stats.number_of_LinphoneCallConnected + 1) and \
                (caller_manager.stats.number_of_LinphoneCallConnected == initial_caller_stats.number_of_LinphoneCallConnected + 1)), True)
        # Just to sleep
        result = CoreManager.wait_for(callee_manager, caller_manager,
            lambda callee_manager, caller_manager: (callee_manager.stats.number_of_LinphoneCallStreamsRunning == initial_callee_stats.number_of_LinphoneCallStreamsRunning + 1) and \
                (caller_manager.stats.number_of_LinphoneCallStreamsRunning == initial_caller_stats.number_of_LinphoneCallStreamsRunning + 1))

        if caller_manager.lc.media_encryption != linphone.MediaEncryption.MediaEncryptionNone and callee_manager.lc.media_encryption != linphone.MediaEncryption.MediaEncryptionNone:
            # Wait for encryption to be on, in case of zrtp, it can take a few seconds
            if caller_manager.lc.media_encryption == linphone.MediaEncryption.MediaEncryptionZRTP:
                CoreManager.wait_for(callee_manager, caller_manager,
                    lambda callee_manager, caller_manager: caller_manager.stats.number_of_LinphoneCallEncryptedOn == initial_caller_stats.number_of_LinphoneCallEncryptedOn + 1)
            if callee_manager.lc.media_encryption == linphone.MediaEncryption.MediaEncryptionZRTP:
                CoreManager.wait_for(callee_manager, caller_manager,
                    lambda callee_manager, caller_manager: callee_manager.stats.number_of_LinphoneCallEncryptedOn == initial_callee_stats.number_of_LinphoneCallEncryptedOn + 1)
            assert_equals(callee_manager.lc.current_call.current_params.media_encryption, caller_manager.lc.media_encryption)
            assert_equals(caller_manager.lc.current_call.current_params.media_encryption, callee_manager.lc.media_encryption)

        return result

    @classmethod
    def end_call(cls, caller_manager, callee_manager):
        caller_manager.lc.terminate_all_calls()
        assert_equals(CoreManager.wait_for(caller_manager, callee_manager,
            lambda caller_manager, callee_manager: caller_manager.stats.number_of_LinphoneCallEnd == 1 and callee_manager.stats.number_of_LinphoneCallEnd == 1), True)

    @classmethod
    def registration_state_changed(cls, lc, cfg, state, message):
        manager = lc.user_data
        if manager.logger is not None:
            manager.logger.info("[TESTER] New registration state {state} for user id [{identity}] at proxy [{addr}]".format(
                state=linphone.RegistrationState.string(state), identity=cfg.identity, addr=cfg.server_addr))
        if state == linphone.RegistrationState.RegistrationNone:
            manager.stats.number_of_LinphoneRegistrationNone += 1
        elif state == linphone.RegistrationState.RegistrationProgress:
            manager.stats.number_of_LinphoneRegistrationProgress += 1
        elif state == linphone.RegistrationState.RegistrationOk:
            manager.stats.number_of_LinphoneRegistrationOk += 1
        elif state == linphone.RegistrationState.RegistrationCleared:
            manager.stats.number_of_LinphoneRegistrationCleared += 1
        elif state == linphone.RegistrationState.RegistrationFailed:
            manager.stats.number_of_LinphoneRegistrationFailed += 1
        else:
            raise Exception("Unexpected registration state")

    @classmethod
    def auth_info_requested(cls, lc, realm, username, domain):
        manager = lc.user_data
        if manager.logger is not None:
            manager.logger.info("[TESTER] Auth info requested  for user id [{username}] at realm [{realm}]".format(
                username=username, realm=realm))
        manager.stats.number_of_auth_info_requested +=1

    @classmethod
    def call_state_changed(cls, lc, call, state, msg):
        manager = lc.user_data
        to_address = call.call_log.to_address.as_string()
        from_address = call.call_log.from_address.as_string()
        direction = "Outgoing"
        if call.call_log.dir == linphone.CallDir.CallIncoming:
            direction = "Incoming"
        if manager.logger is not None:
            manager.logger.info("[TESTER] {direction} call from [{from_address}] to [{to_address}], new state is [{state}]".format(
                direction=direction, from_address=from_address, to_address=to_address, state=linphone.CallState.string(state)))
        if state == linphone.CallState.CallIncomingReceived:
            manager.stats.number_of_LinphoneCallIncomingReceived += 1
        elif state == linphone.CallState.CallOutgoingInit:
            manager.stats.number_of_LinphoneCallOutgoingInit += 1
        elif state == linphone.CallState.CallOutgoingProgress:
            manager.stats.number_of_LinphoneCallOutgoingProgress += 1
        elif state == linphone.CallState.CallOutgoingRinging:
            manager.stats.number_of_LinphoneCallOutgoingRinging += 1
        elif state == linphone.CallState.CallOutgoingEarlyMedia:
            manager.stats.number_of_LinphoneCallOutgoingEarlyMedia += 1
        elif state == linphone.CallState.CallConnected:
            manager.stats.number_of_LinphoneCallConnected += 1
        elif state == linphone.CallState.CallStreamsRunning:
            manager.stats.number_of_LinphoneCallStreamsRunning += 1
        elif state == linphone.CallState.CallPausing:
            manager.stats.number_of_LinphoneCallPausing += 1
        elif state == linphone.CallState.CallPaused:
            manager.stats.number_of_LinphoneCallPaused += 1
        elif state == linphone.CallState.CallResuming:
            manager.stats.number_of_LinphoneCallResuming += 1
        elif state == linphone.CallState.CallRefered:
            manager.stats.number_of_LinphoneCallRefered += 1
        elif state == linphone.CallState.CallError:
            manager.stats.number_of_LinphoneCallError += 1
        elif state == linphone.CallState.CallEnd:
            manager.stats.number_of_LinphoneCallEnd += 1
        elif state == linphone.CallState.CallPausedByRemote:
            manager.stats.number_of_LinphoneCallPausedByRemote += 1
        elif state == linphone.CallState.CallUpdatedByRemote:
            manager.stats.number_of_LinphoneCallUpdatedByRemote += 1
        elif state == linphone.CallState.CallIncomingEarlyMedia:
            manager.stats.number_of_LinphoneCallIncomingEarlyMedia += 1
        elif state == linphone.CallState.CallUpdating:
            manager.stats.number_of_LinphoneCallUpdating += 1
        elif state == linphone.CallState.CallReleased:
            manager.stats.number_of_LinphoneCallReleased += 1
        else:
            raise Exception("Unexpected call state")

    @classmethod
    def message_received(cls, lc, room, message):
        manager = lc.user_data
        from_str = message.from_address.as_string()
        text_str = message.text
        external_body_url = message.external_body_url
        if manager.logger is not None:
            manager.logger.info("[TESTER] Message from [{from_str}] is [{text_str}], external URL [{external_body_url}]".format(
                from_str=from_str, text_str=text_str, external_body_url=external_body_url))
        manager.stats.number_of_LinphoneMessageReceived += 1

        if message.external_body_url is not None:
            manager.stats.number_of_LinphoneMessageExtBodyReceived += 1

    def __init__(self, rc_file = None, check_for_proxies = True, vtable = {}, logger=None):
        self.logger = logger
        if not vtable.has_key('registration_state_changed'):
            vtable['registration_state_changed'] = CoreManager.registration_state_changed
        if not vtable.has_key('auth_info_requested'):
            vtable['auth_info_requested'] = CoreManager.auth_info_requested
        if not vtable.has_key('call_state_changed'):
            vtable['call_state_changed'] = CoreManager.call_state_changed
        if not vtable.has_key('message_received'):
            vtable['message_received'] = CoreManager.message_received
        #if not vtable.has_key('file_transfer_recv'):
            #vtable['file_transfer_recv'] = CoreManager.file_transfer_recv
        #if not vtable.has_key('file_transfer_send'):
            #vtable['file_transfer_send'] = CoreManager.file_transfer_send
        #if not vtable.has_key('file_transfer_progress_indication'):
            #vtable['file_transfer_progress_indication'] = CoreManager.file_transfer_progress_indication
        #if not vtable.has_key('is_composing_received'):
            #vtable['is_composing_received'] = CoreManager.is_composing_received
        #if not vtable.has_key('new_subscription_requested'):
            #vtable['new_subscription_requested'] = CoreManager.new_subscription_requested
        #if not vtable.has_key('notify_presence_received'):
            #vtable['notify_presence_received'] = CoreManager.notify_presence_received
        #if not vtable.has_key('transfer_state_changed'):
            #vtable['transfer_state_changed'] = CoreManager.transfer_state_changed
        #if not vtable.has_key('info_received'):
            #vtable['info_received'] = CoreManager.info_received
        #if not vtable.has_key('subscription_state_changed'):
            #vtable['subscription_state_changed'] = CoreManager.subscription_state_changed
        #if not vtable.has_key('notify_received'):
            #vtable['notify_received'] = CoreManager.notify_received
        #if not vtable.has_key('publish_state_changed'):
            #vtable['publish_state_changed'] = CoreManager.publish_state_changed
        #if not vtable.has_key('configuring_status'):
            #vtable['configuring_status'] = CoreManager.configuring_status
        #if not vtable.has_key('call_encryption_changed'):
            #vtable['call_encryption_changed'] = CoreManager.call_encryption_changed
        self.identity = None
        self.stats = CoreManagerStats()
        rc_path = None
        if rc_file is not None:
            rc_path = os.path.join('rcfiles', rc_file)
        self.lc = self.configure_lc_from(vtable, tester_resources_path, rc_path)
        self.lc.user_data = self
        if check_for_proxies and rc_file is not None:
            proxy_count = len(self.lc.proxy_config_list)
        else:
            proxy_count = 0
        if proxy_count:
            if self.logger is not None:
                self.logger.warning(self)
            CoreManager.wait_for_until(self, None, lambda manager: manager.stats.number_of_LinphoneRegistrationOk == proxy_count, 5000 * proxy_count)
        assert_equals(self.stats.number_of_LinphoneRegistrationOk, proxy_count)
        self.enable_audio_codec("PCMU", 8000)

        if self.lc.default_proxy_config is not None:
            self.identity = linphone.Address.new(self.lc.default_proxy_config.identity)
            self.identity.clean()

    def stop(self):
        self.lc = None

    def configure_lc_from(self, vtable, resources_path, rc_path):
        filepath = None
        if rc_path is not None:
            filepath = os.path.join(resources_path, rc_path)
            assert_equals(os.path.isfile(filepath), True)
        lc = linphone.Core.new(vtable, None, filepath)
        lc.root_ca = os.path.join(resources_path, 'certificates', 'cn', 'cafile.pem')
        lc.ring = os.path.join(resources_path, 'sounds', 'oldphone.wav')
        lc.ringback = os.path.join(resources_path, 'sounds', 'ringback.wav')
        lc.static_picture = os.path.join(resources_path, 'images', 'nowebcamCIF.jpg')
        return lc

    def enable_audio_codec(self, mime, rate):
        codecs = self.lc.audio_codecs
        for codec in codecs:
            self.lc.enable_payload_type(codec, False)
        codec = self.lc.find_payload_type(mime, rate, 1)
        assert codec is not None
        if codec is not None:
            self.lc.enable_payload_type(codec, True)

    def disable_all_audio_codecs_except_one(self, mime):
        self.enable_audio_codec(mime, -1)
