using belle_sip_tester_runtime_component;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

namespace belle_sip_tester_windows10
{
    public delegate void OutputDisplayDelegate(String msg);

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class ResultPage : Page
    {
        public ResultPage()
        {
            this.InitializeComponent();
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            Dictionary<string, string> parameters = e.Parameter as Dictionary<string, string>;
            string suiteName = parameters["suite"];
            string testName = "ALL";
            if (parameters.ContainsKey("test"))
            {
                testName = parameters["test"];
            }
            bool verbose = Convert.ToBoolean(parameters["verbose"]);
            var app = (Application.Current as App);
            app.suiteRunner = new UnitTestSuiteRunner(suiteName, testName, verbose, new OutputDisplayDelegate(OutputDisplay));
            app.suiteRunner.run();
        }

        public async void OutputDisplay(String msg)
        {
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                Results.Text += msg;
            });
        }
    }

    public class UnitTestSuiteRunner : OutputTraceListener
    {
        public UnitTestSuiteRunner(string SuiteName, string CaseName, bool Verbose, OutputDisplayDelegate OutputDisplay)
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
            }, tup);
            await t;
            Running = false;
        }

        public void outputTrace(String msg)
        {
            if (OutputDisplay != null)
            {
                OutputDisplay(msg);
            }
            System.Diagnostics.Debug.WriteLine(msg);
        }

        public bool running
        {
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
