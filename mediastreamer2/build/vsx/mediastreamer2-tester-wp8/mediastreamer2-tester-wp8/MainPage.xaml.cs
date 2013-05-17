using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Phone.Controls;
using Microsoft.Phone.Shell;
using Windows.Phone.Networking.Voip;
using mediastreamer2_tester_wp8.Resources;
using mediastreamer2_tester_native;

namespace mediastreamer2_tester_wp8
{
    public partial class MainPage : PhoneApplicationPage, INotifyPropertyChanged
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

            remoteVideo.DataContext = this;
            this.RemoteVideoUri = null;
            this.RemoteVideoVisibility = Visibility.Collapsed;
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

        protected override void OnNavigatedTo(System.Windows.Navigation.NavigationEventArgs nee)
        {
            base.OnNavigatedTo(nee);

            // Re-bind MediaElements explictly, so video will play after app has been resumed
            remoteVideo.SetBinding(MediaElement.SourceProperty, new System.Windows.Data.Binding("RemoteVideoUri"));
            //localVideo.SetBinding(MediaElement.SourceProperty, new System.Windows.Data.Binding("LocalVideoUri"));
        }

        protected override void OnNavigatedFrom(NavigationEventArgs nee)
        {
            base.OnNavigatedFrom(nee);
        }

        private void Pivot_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var videoRenderer = (Application.Current as App).videoRenderer;

            if (e.AddedItems.Contains(VideoPivot))
            {
                Debug.WriteLine("[MainPage] Starting video");
                msVideo = new Mediastreamer2TesterVideo();
                videoRenderer.Start();
                RemoteVideoUri = remoteStreamUri;
                RemoteVideoVisibility = Visibility.Visible;
            }
            else if (e.RemovedItems.Contains(VideoPivot))
            {
                Debug.WriteLine("[MainPage] Stopping video");
                videoRenderer.Stop();
                msVideo.Dispose();
                msVideo = null;
                RemoteVideoVisibility = Visibility.Collapsed;
                RemoteVideoUri = null;
            }
        }

        private void remoteVideo_MediaOpened_1(object sender, System.Windows.RoutedEventArgs e)
        {
            Debug.WriteLine("[MainPage] RemoteVideo Opened: " + ((MediaElement)sender).Source.AbsoluteUri);
        }

        private void remoteVideo_MediaFailed_1(object sender, System.Windows.ExceptionRoutedEventArgs e)
        {
            Debug.WriteLine("[MainPage] RemoteVideo Failed: " + e.ErrorException.Message);
        }

        private void remoteVideo_CurrentStateChanged(object sender, RoutedEventArgs e)
        {
            Debug.WriteLine("[MainPage] RemoteVideo State: " + remoteVideo.CurrentState.ToString());
        }

        private void localVideo_MediaOpened_1(object sender, System.Windows.RoutedEventArgs e)
        {
            Debug.WriteLine("[MainPage] LocalVideo Opened: " + ((MediaElement)sender).Source.AbsoluteUri);
        }

        private void localVideo_MediaFailed_1(object sender, System.Windows.ExceptionRoutedEventArgs e)
        {
            Debug.WriteLine("[MainPage] LocalVideo Failed: " + e.ErrorException.Message);
        }


        #region INotifyPropertyChanged Members

        public event PropertyChangedEventHandler PropertyChanged;

        protected void OnPropertyChanged(string name)
        {
            if (this.PropertyChanged != null)
            {
                this.PropertyChanged(this, new PropertyChangedEventArgs(name));
            }
        }

        #endregion
        
        
        private Uri remoteVideoUri;
        public Uri RemoteVideoUri
        {
            get
            {
                return this.remoteVideoUri;
            }

            set
            {
                if (this.remoteVideoUri != value)
                {
                    this.remoteVideoUri = value;
                    this.OnPropertyChanged("RemoteVideoUri");
                }
            }
        }

        private Visibility remoteVideoVisibility;
        public Visibility RemoteVideoVisibility
        {
            get
            {
                return this.remoteVideoVisibility;
            }

            set
            {
                if (this.remoteVideoVisibility != value)
                {
                    this.remoteVideoVisibility = value;
                    this.OnPropertyChanged("RemoteVideoVisibility");
                }
            }
        }


        private Mediastreamer2TesterVideo msVideo = null;

        private static Uri remoteStreamUri = new Uri("ms-media-stream-id:MediaStreamer-5060");
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