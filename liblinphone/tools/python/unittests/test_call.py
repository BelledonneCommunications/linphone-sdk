from nose.tools import assert_equals
import linphone
from linphonetester import *
import os
import time


class TestCall:

    @classmethod
    def setup_class(cls):
        base, ext = os.path.splitext(os.path.basename(__file__))
        cls.logger = Logger(base + '.log')

    def test_early_declined_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        marie.lc.max_calls = 0
        out_call = pauline.lc.invite_address(marie.identity)

        # Wait until flexisip transfers the busy...
        assert_equals(CoreManager.wait_for_until(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallError == 1, 33000), True)
        assert_equals(pauline.stats.number_of_LinphoneCallError, 1)
        #FIXME http://git.linphone.org/mantis/view.php?id=757
        #assert_equals(out_call.reason, linphone.Reason.ReasonBusy)
        if len(pauline.lc.call_logs) > 0:
            out_call_log = pauline.lc.call_logs[0]
            assert out_call_log is not None
            assert_equals(out_call_log.status, linphone.CallStatus.CallAborted)
        marie.stop()
        pauline.stop()

    def test_declined_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        out_call = pauline.lc.invite_address(marie.identity)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: marie.stats.number_of_LinphoneCallIncomingReceived == 1), True)
        in_call = marie.lc.current_call
        assert in_call is not None
        if in_call is not None:
            marie.lc.terminate_call(in_call)
            assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: marie.stats.number_of_LinphoneCallReleased == 1), True)
            assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallReleased == 1), True)
            assert_equals(marie.stats.number_of_LinphoneCallEnd, 1)
            assert_equals(pauline.stats.number_of_LinphoneCallEnd, 1)
            assert_equals(in_call.reason, linphone.Reason.ReasonDeclined)
            assert_equals(out_call.reason, linphone.Reason.ReasonDeclined)
        marie.stop()
        pauline.stop()

    def test_cancelled_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        out_call = pauline.lc.invite_address(marie.identity)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallOutgoingInit == 1), True)
        pauline.lc.terminate_call(out_call)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallEnd == 1), True)
        assert_equals(pauline.stats.number_of_LinphoneCallEnd, 1)
        assert_equals(marie.stats.number_of_LinphoneCallIncomingReceived, 0)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallReleased == 1), True)
        marie.stop()
        pauline.stop()

    def test_early_cancelled_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('empty_rc', check_for_proxies=False, logger=TestCall.logger)
        out_call = pauline.lc.invite_address(marie.identity)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallOutgoingInit == 1), True)
        pauline.lc.terminate_call(out_call)

        # Since everything is executed in a row, no response can be received from the server, thus the CANCEL cannot be sent.
        # It will ring at Marie's side.
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallEnd == 1), True)
        assert_equals(pauline.stats.number_of_LinphoneCallEnd, 1)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: marie.stats.number_of_LinphoneCallIncomingReceived == 1), True)

        # Now the CANCEL should have been sent and the the call at marie's side should terminate
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: marie.stats.number_of_LinphoneCallEnd == 1), True)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallReleased == 1), True)
        marie.stop()
        pauline.stop()

    def test_cancelled_ringing_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        out_call = pauline.lc.invite_address(marie.identity)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: marie.stats.number_of_LinphoneCallIncomingReceived == 1), True)
        pauline.lc.terminate_call(out_call)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: (pauline.stats.number_of_LinphoneCallReleased == 1) and (marie.stats.number_of_LinphoneCallReleased == 1)), True)
        assert_equals(marie.stats.number_of_LinphoneCallEnd, 1)
        assert_equals(pauline.stats.number_of_LinphoneCallEnd, 1)
        marie.stop()
        pauline.stop()

    def test_call_failed_because_of_codecs(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        marie.disable_all_audio_codecs_except_one('pcmu')
        pauline.disable_all_audio_codecs_except_one('pcma')
        out_call = pauline.lc.invite_address(marie.identity)
        assert_equals(CoreManager.wait_for(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallOutgoingInit == 1), True)

        # flexisip will retain the 488 until the "urgent reply" timeout arrives.
        assert_equals(CoreManager.wait_for_until(pauline, marie, lambda pauline, marie: pauline.stats.number_of_LinphoneCallError == 1, 7000), True)
        assert_equals(out_call.reason, linphone.Reason.ReasonNotAcceptable)
        assert_equals(marie.stats.number_of_LinphoneCallIncomingReceived, 0)
        assert_equals(marie.stats.number_of_LinphoneCallReleased, 0)
        marie.stop()
        pauline.stop()

    def test_simple_call(self):
        marie = CoreManager('marie_rc', logger=TestCall.logger)
        pauline = CoreManager('pauline_rc', logger=TestCall.logger)
        assert_equals(CoreManager.call(pauline, marie), True)
        #liblinphone_tester_check_rtcp(marie,pauline);
        CoreManager.end_call(marie, pauline)
        marie.stop()
        pauline.stop()
