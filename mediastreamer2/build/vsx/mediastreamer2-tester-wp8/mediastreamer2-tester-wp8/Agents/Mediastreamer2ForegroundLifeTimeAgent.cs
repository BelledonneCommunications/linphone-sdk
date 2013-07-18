using Microsoft.Phone.Networking.Voip;
using System.Diagnostics;
using System.Threading;

namespace Mediastreamer2.Agents
{
    public class Mediastreamer2ForegroundLifeTimeAgent : VoipForegroundLifetimeAgent
    {
        public Mediastreamer2ForegroundLifeTimeAgent() : base()
        {

        }

        /// <summary>
        /// Called when the app is in foreground (when it starts or when it's resumed)
        /// </summary>
        protected override void OnLaunched()
        {
            Debug.WriteLine("[Mediastreamer2ForegroundLifeTimeAgent] The UI has entered the foreground.");
        }

        /// <summary>
        /// Called when the app is in background
        /// </summary>
        protected override void OnCancel()
        {
            Debug.WriteLine("[Mediastreamer2ForegroundLifeTimeAgent] The UI is leaving the foreground");
            base.NotifyComplete();
        }
    }
}