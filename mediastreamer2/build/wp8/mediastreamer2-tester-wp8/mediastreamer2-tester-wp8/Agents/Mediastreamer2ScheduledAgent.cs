using Microsoft.Phone.Networking.Voip;
using Microsoft.Phone.Scheduler;
using System;
using System.Diagnostics;

namespace Mediastreamer2.Agents
{
    public class Mediastreamer2ScheduledAgent : ScheduledTaskAgent
    {
        // Indicates if this agent instance is handling an incoming call or not
        private bool isIncomingCallAgent;

        public Mediastreamer2ScheduledAgent()
        {

        }

        protected override void OnInvoke(ScheduledTask task)
        {
            Debug.WriteLine("[Mediastreamer2ScheduledAgent] ScheduledAgentImpl has been invoked with argument of type {0}.", task.GetType());
            VoipHttpIncomingCallTask incomingCallTask = task as VoipHttpIncomingCallTask;
            if (incomingCallTask != null)
            {
                this.isIncomingCallAgent = true;
                Debug.WriteLine("[IncomingCallAgent] Received VoIP Incoming Call task");
            }
            else
            {
                VoipKeepAliveTask keepAliveTask = task as VoipKeepAliveTask;
                if (keepAliveTask != null)
                {
                    this.isIncomingCallAgent = false;
                    Debug.WriteLine("[KeepAliveAgent] Keep Alive task");
                    base.NotifyComplete();
                }
                else
                {
                    throw new InvalidOperationException(string.Format("Unknown scheduled task type {0}", task.GetType()));
                }
            }
        }

        // This method is called when the incoming call processing is complete to kill the background process if needed
        private void OnIncomingCallDialogDismissed()
        {
            Debug.WriteLine("[IncomingCallAgent] Incoming call processing is now complete.");
            base.NotifyComplete();
        } 

        protected override void OnCancel()
        {
            Debug.WriteLine("[{0}] Cancel requested.", this.isIncomingCallAgent ? "IncomingCallAgent" : "KeepAliveAgent");
            base.NotifyComplete();
        }
    }
}
