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