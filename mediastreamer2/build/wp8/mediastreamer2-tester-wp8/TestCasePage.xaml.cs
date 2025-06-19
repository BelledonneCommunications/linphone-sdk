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
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;

namespace mediastreamer2_tester_wp8
{
    public partial class TestCasePage : PhoneApplicationPage
    {
        public TestCasePage()
        {
            InitializeComponent();
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            suiteName = NavigationContext.QueryString["SuiteName"];
            verbose = Convert.ToBoolean(NavigationContext.QueryString["Verbose"]);
            var tester = (Application.Current as App).tester;
            List<UnitTestCaseName> source = new List<UnitTestCaseName>();
            source.Add(new UnitTestCaseName("ALL"));
            for (int i = 0; i < tester.nbTests(suiteName); i++)
            {
                source.Add(new UnitTestCaseName(tester.testName(suiteName, i)));
            }

            Tests.ItemsSource = source;
        }

        private void Tests_Tap(object sender, System.Windows.Input.GestureEventArgs e)
        {
            UnitTestCaseName test = (sender as LongListSelector).SelectedItem as UnitTestCaseName;
            if (test == null) return;
            if (!(Application.Current as App).suiteRunning())
            {
                NavigationService.Navigate(new Uri("/TestResultPage.xaml?SuiteName=" + suiteName + "&CaseName=" + test.Name + "&Verbose=" + verbose, UriKind.Relative));
            }
        }

        private string suiteName;
        private bool verbose;
    }

    public class UnitTestCaseName
    {
        public string Name
        {
            get;
            set;
        }

        public UnitTestCaseName(string name)
        {
            this.Name = name;
        }
    }
}