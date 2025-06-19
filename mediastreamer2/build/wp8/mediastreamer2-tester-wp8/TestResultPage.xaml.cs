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
﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;
using mediastreamer2_tester_native;
using mediastreamer2_tester_wp8;

namespace mediastreamer2_tester_wp8
{
    public delegate void OutputDisplayDelegate(int level, String msg);

    public partial class TestResultPage : PhoneApplicationPage
    {
        public TestResultPage()
        {
            InitializeComponent();
            Browser.Navigate(new Uri("log.html", UriKind.Relative));
        }

        private void Browser_LoadCompleted(object sender, NavigationEventArgs e)
        {
            string suiteName = NavigationContext.QueryString["SuiteName"];
            string caseName;
            if (NavigationContext.QueryString.ContainsKey("CaseName"))
            {
                caseName = NavigationContext.QueryString["CaseName"];
            }
            else
            {
                caseName = "ALL";
            }
            bool verbose = Convert.ToBoolean(NavigationContext.QueryString["Verbose"]);
            var app = (Application.Current as App);
            app.suite = new UnitTestSuite(suiteName, caseName, verbose, new OutputDisplayDelegate(OutputDisplay));
            app.suite.run();
        }

        public void OutputDisplay(int level, String msg)
        {
            this.Dispatcher.BeginInvoke(() =>
                {
                    msg = msg.Replace("\r\n", "\n");
                    string[] lines = msg.Split('\n');
                    bool insertNewLine = false;
                    foreach (string line in lines)
                    {
                        if (line.Length == 0)
                        {
                            insertNewLine = false;
                            Browser.InvokeScript("append_nl");
                        }
                        else
                        {
                            if (insertNewLine == true)
                            {
                                Browser.InvokeScript("append_nl");
                            }
                            if (level == 0)
                            {
                                Browser.InvokeScript("append_trace", line, "debug");
                            }
                            else if (level == 1)
                            {
                                Browser.InvokeScript("append_trace", line, "message");
                            }
                            else if (level == 2)
                            {
                                Browser.InvokeScript("append_trace", line, "warning");
                            }
                            else if (level == 3)
                            {
                                Browser.InvokeScript("append_trace", line, "error");
                            }
                            else
                            {
                                Browser.InvokeScript("append_text", line);
                            }
                            insertNewLine = true;
                        }
                    }
                });
        }
    }

    public class UnitTestSuite : OutputTraceListener
    {
        public UnitTestSuite(string SuiteName, string CaseName, bool Verbose, OutputDisplayDelegate OutputDisplay)
        {
            this.SuiteName = SuiteName;
            this.CaseName = CaseName;
            this.Verbose = Verbose;
            this.Running = false;
            this.OutputDisplay = OutputDisplay;
        }

        async public void run()
        {
            Running = true;
            var tup = new Tuple<string, string, bool>(SuiteName, CaseName, Verbose);
            var t = Task.Factory.StartNew((object parameters) =>
            {
                var tester = (Application.Current as App).tester;
                tester.setOutputTraceListener(this);
                var p = parameters as Tuple<string, string, bool>;
                tester.run(p.Item1, p.Item2, p.Item3);
                return true;
            }, tup);
            await t;
            Running = false;
        }

        public void outputTrace(int level, String msg)
        {
            if (OutputDisplay != null)
            {
                OutputDisplay(level, msg);
            }
            System.Diagnostics.Debug.WriteLine(msg);
        }

        public bool running {
            get { return Running; }
            protected set { Running = value; }
        }

        private string SuiteName;
        private string CaseName;
        private bool Verbose;
        private bool Running;
        private OutputDisplayDelegate OutputDisplay;
    }
}