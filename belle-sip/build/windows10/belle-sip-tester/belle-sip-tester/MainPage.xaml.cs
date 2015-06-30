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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace belle_sip_tester
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            this.InitializeComponent();
            Suites.DataContext = new UnitTestSuites();
        }

        private void Suite_Tapped(object sender, TappedRoutedEventArgs e)
        {
            UnitTestSuite suite = (sender as ListView).SelectedItem as UnitTestSuite;
            if (suite == null) return;
            Dictionary<string, string> parameters = new Dictionary<string, string>();
            parameters.Add("suite", suite.Name);
            parameters.Add("verbose", Verbose.IsChecked.ToString());
            if (suite.Name == "ALL")
            {
                Frame.Navigate(typeof(ResultPage), parameters);
            }
            else
            {
                Frame.Navigate(typeof(TestPage), parameters);
            }
        }
    }

    public class UnitTestSuite
    {
        public string Name
        {
            get;
            set;
        }

        public UnitTestSuite(string name)
        {
            this.Name = name;
        }
    }

    public class UnitTestSuites : ObservableCollection<UnitTestSuite>
    {
        public UnitTestSuites()
        {
            var tester = (Application.Current as App).tester;
            Add(new UnitTestSuite("ALL"));
            for (int i = 0; i < tester.nbTestSuites(); i++)
            {
                Add(new UnitTestSuite(tester.testSuiteName(i)));
            }
        }
    }
}
