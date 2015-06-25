using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
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
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class TestPage : Page
    {
        public TestPage()
        {
            this.InitializeComponent();
        }

        override protected void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            Dictionary<string, string> parameters = e.Parameter as Dictionary<string, string>;
            SuiteName.Text = parameters["suite"];
            Verbose.IsChecked = Convert.ToBoolean(parameters["verbose"]);
            Tests.DataContext = new UnitTestCases(SuiteName.Text);
        }

        private void Test_Tapped(object sender, TappedRoutedEventArgs e)
        {
            UnitTestCase test = (sender as ListView).SelectedItem as UnitTestCase;
            if (test == null) return;
            //test.Name = test.Name.Replace("+", "%2B").Replace(" ", "%20");
            if (!(Application.Current as App).suiteRunning())
            {
                Dictionary<string, string> parameters = new Dictionary<string, string>();
                parameters.Add("suite", SuiteName.Text);
                parameters.Add("test", test.Name);
                parameters.Add("verbose", Verbose.IsChecked.ToString());
                Frame.Navigate(typeof(ResultPage), parameters);
            }
        }
    }

    public class UnitTestCase
    {
        public string Name
        {
            get;
            set;
        }

        public UnitTestCase(string name)
        {
            this.Name = name;
        }
    }

    public class UnitTestCases : ObservableCollection<UnitTestCase>
    {
        public UnitTestCases(string suiteName)
        {
            var tester = (Application.Current as App).tester;
            Add(new UnitTestCase("ALL"));
            for (int i = 0; i < tester.nbTests(suiteName); i++)
            {
                Add(new UnitTestCase(tester.testName(suiteName, i)));
            }
        }
    }
}
