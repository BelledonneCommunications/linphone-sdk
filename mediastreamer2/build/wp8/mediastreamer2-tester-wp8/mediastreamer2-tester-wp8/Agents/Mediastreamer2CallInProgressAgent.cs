using Microsoft.Phone.Networking.Voip;
using System.Diagnostics;

namespace Mediastreamer2.Agents
{
    public class Mediastreamer2CallInProgressAgent : VoipCallInProgressAgent
    {
        public Mediastreamer2CallInProgressAgent() : base()
        {
        }

        /// <summary>
        /// Called when the first call has started.
        /// </summary>
        protected override void OnFirstCallStarting()
        {
            Debug.WriteLine("[Mediastreamer2CallInProgressAgent] The first call has started.");
        }

        /// <summary>
        /// Called when the last call has ended.
        /// </summary>
        protected override void OnCancel()
        {
            Debug.WriteLine("[Mediastreamer2CallInProgressAgent] The last call has ended. Calling NotifyComplete");
            base.NotifyComplete();
        }
    }
}