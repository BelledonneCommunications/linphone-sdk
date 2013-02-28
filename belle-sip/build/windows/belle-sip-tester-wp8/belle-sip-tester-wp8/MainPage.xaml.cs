using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;
using belle_sip_tester_wp8.Resources;

namespace belle_sip_tester_wp8
{
    public partial class MainPage : PhoneApplicationPage
    {
        // Constructor
        public MainPage()
        {
            InitializeComponent();

            var tester = (Application.Current as App).tester;
            List<UnitTestSuiteName> source = new List<UnitTestSuiteName>();
            source.Add(new UnitTestSuiteName("ALL"));
            for (int i = 0; i < tester.nbTestSuites(); i++)
            {
                source.Add(new UnitTestSuiteName(tester.testSuiteName(i)));
            }

            Tests.ItemsSource = source;

            // Sample code to localize the ApplicationBar
            //BuildLocalizedApplicationBar();
        }

        private void Tests_Tap(object sender, System.Windows.Input.GestureEventArgs e)
        {
            UnitTestSuiteName test = (sender as LongListSelector).SelectedItem as UnitTestSuiteName;
            if (test == null) return;
            if (test.Name == "ALL")
            {
                NavigationService.Navigate(new Uri("/TestResultPage.xaml?SuiteName=" + test.Name + "&Verbose=" + Verbose.IsChecked.GetValueOrDefault(), UriKind.Relative));
            }
            else
            {
                NavigationService.Navigate(new Uri("/TestCasePage.xaml?SuiteName=" + test.Name + "&Verbose=" + Verbose.IsChecked.GetValueOrDefault(), UriKind.Relative));
            }
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