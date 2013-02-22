using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;
using mediastreamer2_tester_native;
using mediastreamer2_tester_wp8;

namespace cain_sip_tester_wp8
{
    public delegate void OutputDisplayDelegate(String msg);

    public partial class TestResultPage : PhoneApplicationPage
    {
        public TestResultPage()
        {
            InitializeComponent();
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            string suiteName = NavigationContext.QueryString["SuiteName"];
            bool verbose = Convert.ToBoolean(NavigationContext.QueryString["Verbose"]);
            var suite = new UnitTestSuite(suiteName, verbose, new OutputDisplayDelegate(OutputDisplay));
            suite.run();
            ;
        }

        public void OutputDisplay(String msg)
        {
            this.Dispatcher.BeginInvoke(() =>
                {
                    TestResults.Text += msg;
                });
        }
    }

    public class UnitTestSuite : OutputTraceListener
    {
        public UnitTestSuite(string SuiteName, bool Verbose, OutputDisplayDelegate OutputDisplay)
        {
            this.SuiteName = SuiteName;
            this.Verbose = Verbose;
            this.OutputDisplay = OutputDisplay;
        }

        async public void run()
        {
            var tup = new Tuple<String, bool>(SuiteName, Verbose);
            var t = Task.Factory.StartNew((object parameters) =>
            {
                var tester = (Application.Current as App).tester;
                tester.setOutputTraceListener(this);
                var p = parameters as Tuple<String, bool>;
                tester.run(p.Item1, p.Item2);
            }, tup);
            await t;
        }

        public void outputTrace(String msg)
        {
            if (OutputDisplay != null)
            {
                OutputDisplay(msg);
            }
            System.Diagnostics.Debug.WriteLine(msg);
        }

        private string SuiteName;
        private bool Verbose;
        private OutputDisplayDelegate OutputDisplay;
    }


}