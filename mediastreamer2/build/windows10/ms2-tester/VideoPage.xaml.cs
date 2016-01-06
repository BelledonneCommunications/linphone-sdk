using ms2_tester.Helpers;
using ms2_tester_runtime_component;
using MS2TesterTasks;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Devices.Sensors;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Graphics.Display;
using Windows.UI.ViewManagement;
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

        override protected void OnNavigatedTo(Windows.UI.Xaml.Navigation.NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            displayOrientation = ApplicationView.GetForCurrentView().Orientation;
            displayInformation = DisplayInformation.GetForCurrentView();
            deviceOrientation = SimpleOrientation.NotRotated;
            orientationSensor = SimpleOrientationSensor.GetDefault();
            if (orientationSensor != null)
            {
                deviceOrientation = orientationSensor.GetCurrentOrientation();
                orientationSensor.OrientationChanged += OrientationSensor_OrientationChanged;
            }
            Window.Current.SizeChanged += Current_SizeChanged;
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            base.OnNavigatedFrom(e);
            if (orientationSensor != null)
            {
                orientationSensor.OrientationChanged -= OrientationSensor_OrientationChanged;
            }
            Window.Current.SizeChanged -= Current_SizeChanged;
        }

        private async void OrientationSensor_OrientationChanged(SimpleOrientationSensor sender, SimpleOrientationSensorOrientationChangedEventArgs args)
        {
            // Keep previous orientation when the user puts its device faceup or facedown
            if ((args.Orientation != SimpleOrientation.Faceup) && (args.Orientation != SimpleOrientation.Facedown))
            {
                deviceOrientation = args.Orientation;
                await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => SetVideoOrientation());
            }
        }

        private async void Current_SizeChanged(object sender, Windows.UI.Core.WindowSizeChangedEventArgs e)
        {
            displayOrientation = ApplicationView.GetForCurrentView().Orientation;
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => SetVideoOrientation());
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
                //MS2Tester.Instance.startVideoStream(LocalVideo, RemoteVideo, camera, codec, videoSize, frameRate, bitRate);
                StartVideoStream(camera, codec, videoSize, frameRate, bitRate);
            }
            else
            {
                MS2Tester.Instance.stopVideoStream();
            }
        }

        private async void StartVideoStream(String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            try
            {
                OperationResult result = await MS2TesterHelper.StartVideoStreamAsync(camera, codec, videoSize, frameRate, bitRate);
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("StartVideoStream: success");
                }
                else
                {
                    Debug.WriteLine("StartVideoStream: failure");
                }
            }
            catch (Exception /*e*/)
            {
                //if (e.HResult == MethodCallUnexpectedTime)
                //{
                Debug.WriteLine("StartVideoStream: Async operation already in progress");
                //}
            }
        }

        private void ChangeCameraButton_Click(object sender, RoutedEventArgs e)
        {
            String camera = (CameraComboBox.SelectedItem as ComboBoxItem).Content as String;
            MS2Tester.Instance.changeCamera(camera);
        }

        private void SetVideoOrientation()
        {
            SimpleOrientation orientation = deviceOrientation;
            if (displayInformation.NativeOrientation == DisplayOrientations.Portrait)
            {
                switch (orientation)
                {
                    case SimpleOrientation.Rotated90DegreesCounterclockwise:
                        orientation = SimpleOrientation.NotRotated;
                        break;
                    case SimpleOrientation.Rotated180DegreesCounterclockwise:
                        orientation = SimpleOrientation.Rotated90DegreesCounterclockwise;
                        break;
                    case SimpleOrientation.Rotated270DegreesCounterclockwise:
                        orientation = SimpleOrientation.Rotated180DegreesCounterclockwise;
                        break;
                    case SimpleOrientation.NotRotated:
                    default:
                        orientation = SimpleOrientation.Rotated270DegreesCounterclockwise;
                        break;
                }
            }
            int degrees = 0;
            switch (orientation)
            {
                case SimpleOrientation.Rotated90DegreesCounterclockwise:
                    degrees = 90;
                    break;
                case SimpleOrientation.Rotated180DegreesCounterclockwise:
                    degrees = 180;
                    break;
                case SimpleOrientation.Rotated270DegreesCounterclockwise:
                    degrees = 270;
                    break;
                case SimpleOrientation.NotRotated:
                default:
                    degrees = 0;
                    break;
            }

            if (MS2Tester.Instance.getOrientation() != degrees)
            {
                MS2Tester.Instance.setOrientation(degrees);
            }
        }


        private ApplicationViewOrientation displayOrientation;
        private DisplayInformation displayInformation;
        private SimpleOrientationSensor orientationSensor;
        private SimpleOrientation deviceOrientation;
    }
}
