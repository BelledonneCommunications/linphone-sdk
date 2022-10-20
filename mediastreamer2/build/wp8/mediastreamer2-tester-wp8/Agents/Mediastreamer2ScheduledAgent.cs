/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
﻿using Microsoft.Phone.Networking.Voip;
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
