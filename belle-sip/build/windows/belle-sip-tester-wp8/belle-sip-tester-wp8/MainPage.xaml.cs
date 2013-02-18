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

            List<UnitTest> source = new List<UnitTest>();
            source.Add(new UnitTest("ALL"));
            source.Add(new UnitTest("Authentication-helper"));
            source.Add(new UnitTest("Dialog"));
            source.Add(new UnitTest("Headers"));
            source.Add(new UnitTest("Message"));
            source.Add(new UnitTest("Object inheritence"));
            source.Add(new UnitTest("Refresher"));
            source.Add(new UnitTest("Register"));
            source.Add(new UnitTest("Resolver"));
            source.Add(new UnitTest("SDP"));
            source.Add(new UnitTest("Uri"));

            Tests.ItemsSource = source;
            Tests.SelectionChanged += new SelectionChangedEventHandler(tests_selectionChanged);

            // Sample code to localize the ApplicationBar
            //BuildLocalizedApplicationBar();
        }

        void tests_selectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UnitTest test = (sender as LongListSelector).SelectedItem as UnitTest;
            var tup = new Tuple<String, bool>(test.Name, Verbose.IsChecked.GetValueOrDefault());
            var t = Task.Factory.StartNew((object parameters) =>
                {
                    var tester = new CainSipTesterNative();
                    var p = parameters as Tuple<String, bool>;
                    tester.run(p.Item1, p.Item2);
                }, tup);
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

    public class UnitTest
    {
        public string Name
        {
            get;
            set;
        }

        public UnitTest(string name)
        {
            this.Name = name;
        }
    }
}