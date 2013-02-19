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
using belle_sip_tester_wp8.Resources;
using belle_sip_tester_native;

namespace belle_sip_tester_wp8
{
    public partial class MainPage : PhoneApplicationPage
    {
        // Constructor
        public MainPage()
        {
            InitializeComponent();

            List<UnitTestSuiteName> source = new List<UnitTestSuiteName>();
            source.Add(new UnitTestSuiteName("ALL"));
            source.Add(new UnitTestSuiteName("Authentication-helper"));
            source.Add(new UnitTestSuiteName("Dialog"));
            source.Add(new UnitTestSuiteName("Headers"));
            source.Add(new UnitTestSuiteName("Message"));
            source.Add(new UnitTestSuiteName("Object inheritence"));
            source.Add(new UnitTestSuiteName("Refresher"));
            source.Add(new UnitTestSuiteName("Register"));
            source.Add(new UnitTestSuiteName("Resolver"));
            source.Add(new UnitTestSuiteName("SDP"));
            source.Add(new UnitTestSuiteName("Uri"));

            Tests.ItemsSource = source;
            Tests.SelectionChanged += tests_selectionChanged;

            // Sample code to localize the ApplicationBar
            //BuildLocalizedApplicationBar();
        }

        void tests_selectionChanged(object sender, EventArgs e)
        {
            (sender as LongListSelector).Visibility = Visibility.Collapsed;
            UnitTestSuiteName test = (sender as LongListSelector).SelectedItem as UnitTestSuiteName;
            var suite = new UnitTestSuite(test, Verbose.IsChecked.GetValueOrDefault());
            suite.run();
            (sender as LongListSelector).Visibility = Visibility.Visible;
        }

        // Sample code for building a localized ApplicationBar
        //private void BuildLocalizedApplicationBar()
        //{
        //    // Set the page's ApplicationBar to a new instance of ApplicationBar.
        //    ApplicationBar = new ApplicationBar();

        //    // Create a new button and set the text value to the localized string from AppResources.
        //    ApplicationBarIconButton appBarButton = new ApplicationBarIconButton(new Uri("/Assets/AppBar/appbar.add.rest.png", UriKind.Relative));
        //    appBarButton.Text = AppResources.AppBarButtonText;
        //    ApplicationBar.Buttons.Add(appBarButton);

        //    // Create a new menu item with the localized string from AppResources.
        //    ApplicationBarMenuItem appBarMenuItem = new ApplicationBarMenuItem(AppResources.AppBarMenuItemText);
        //    ApplicationBar.MenuItems.Add(appBarMenuItem);
        //}
    }

    public class UnitTestSuite : OutputTraceListener
    {
        public UnitTestSuite(UnitTestSuiteName test, bool verbose)
        {
            this.test = test;
            this.verbose = verbose;
        }

        async public void run()
        {
            var tup = new Tuple<String, bool>(test.Name, verbose);
            var t = Task.Factory.StartNew((object parameters) =>
            {
                var tester = new CainSipTesterNative(this);
                var p = parameters as Tuple<String, bool>;
                tester.run(p.Item1, p.Item2);
            }, tup);
            await t;
        }

        public void outputTrace(String msg)
        {
            System.Diagnostics.Debug.WriteLine(msg);
        }

        private UnitTestSuiteName test;
        private bool verbose;
    }

    public class UnitTestSuiteName
    {
        public string Name
        {
            get;
            set;
        }

        public UnitTestSuiteName(string name)
        {
            this.Name = name;
        }
    }
}