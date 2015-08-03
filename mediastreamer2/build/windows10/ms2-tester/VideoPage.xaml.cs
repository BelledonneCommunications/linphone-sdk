using ms2_tester_runtime_component;
using System;
using System.Collections.Generic;
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

namespace ms2_tester
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class VideoPage : Page
    {
        public VideoPage()
        {
            this.InitializeComponent();
            MS2Tester.Instance.initVideo();
            bool isSelected = true;
            foreach (String device in MS2Tester.Instance.VideoDevices)
            {
                ComboBoxItem item = new ComboBoxItem();
                item.Content = device;
                item.IsSelected = isSelected;
                isSelected = false;
                CameraComboBox.Items.Add(item);
            }
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            MS2Tester.Instance.uninitVideo();
            ((Frame)Window.Current.Content).GoBack();
        }

        private void RemoteVideo_MediaFailed(object sender, ExceptionRoutedEventArgs e)
        {
            System.Diagnostics.Debug.WriteLine("RemoteVideo_MediaFailed");
        }

        private void RemoteVideo_MediaEnded(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Debug.WriteLine("RemoteVideo_MediaEnded");
        }

        private void RemoteVideo_MediaOpened(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Debug.WriteLine("RemoteVideo_MediaOpened");
        }

        private void RemoteVideo_PartialMediaFailureDetected(MediaElement sender, PartialMediaFailureDetectedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("RemoteVideo_PartialMediaFailureDetected");
        }

        private void RemoteVideo_RateChanged(object sender, RateChangedRoutedEventArgs e)
        {
            System.Diagnostics.Debug.WriteLine("RemoteVideo_RateChanged");
        }

        private void RemoteVideo_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            System.Diagnostics.Debug.WriteLine(String.Format("RemoteVideo_SizeChanged from {0}x{1} to {2}x{3}", e.PreviousSize.Width, e.PreviousSize.Height, e.NewSize.Width, e.NewSize.Height));
        }

        private void RemoteVideo_CurrentStateChanged(object sender, RoutedEventArgs e)
        {
            MediaElement mediaElement = sender as MediaElement;
            System.Diagnostics.Debug.WriteLine(String.Format("RemoteVideo_CurrentStateChanged: {0}", mediaElement.CurrentState));
        }

        private void VideoToggleButton_Checked(object sender, RoutedEventArgs e)
        {
            ToggleButton b = sender as ToggleButton;
            if (b.IsChecked == true)
            {
                String camera = (CameraComboBox.SelectedItem as ComboBoxItem).Content as String;
                String codec = (CodecComboBox.SelectedItem as ComboBoxItem).Content as String;
                String videoSize = (VideoSizeComboBox.SelectedItem as ComboBoxItem).Content as String;
                UInt32 frameRate = 25;
                UInt32.TryParse((FramerateComboBox.SelectedItem as ComboBoxItem).Content as String, out frameRate);
                UInt32 bitRate = 1500;
                UInt32.TryParse(BitrateTextBox.Text, out bitRate);
                MS2Tester.Instance.startVideoStream(LocalVideo, RemoteVideo, camera, codec, videoSize, frameRate, bitRate);
            }
            else
            {
                MS2Tester.Instance.stopVideoStream();
            }
        }
    }
}
