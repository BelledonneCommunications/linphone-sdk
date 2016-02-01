using ms2_tester.Helpers;
using ms2_tester_runtime_component;
using MS2TesterTasks;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel.Calls;
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
            InitVideoPage();
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
            this.Loaded += VideoPage_Loaded;
        }

        private void VideoPage_Loaded(object sender, RoutedEventArgs e)
        {
            AdaptVideoSize();
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
            AdaptVideoSize();
            displayOrientation = ApplicationView.GetForCurrentView().Orientation;
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => SetVideoOrientation());
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            UninitVideo();
            ((Frame)Window.Current.Content).GoBack();
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
                StartVideoStream(camera, codec, videoSize, frameRate, bitRate);
            }
            else
            {
                if (_videoSource != null)
                {
                    _videoSource.Stop();
                    _videoSource = null;
                }
                if (_previewSource != null)
                {
                    _previewSource.Stop();
                    _previewSource = null;
                }
                StopVideoStream();
            }
        }

        private async void InitVideoPage()
        {
            // Perform the video capture & display in a background task. Set to false to run in foreground.
            MS2TesterHelper.RunInBackground = true;

            await InitVideo();
            await FillCameraComboBox();
        }

        private async Task FillCameraComboBox()
        {
            bool isSelected = true;
            List<String> videoDevices = await GetVideoDevices();
            foreach (String device in videoDevices)
            {
                ComboBoxItem item = new ComboBoxItem();
                item.Content = device;
                item.IsSelected = isSelected;
                isSelected = false;
                CameraComboBox.Items.Add(item);
            }
        }

        private async Task InitVideo()
        {
            try
            {
                OperationResult result = await MS2TesterHelper.InitVideo();
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("InitVideo: success");
                } else
                {
                    Debug.WriteLine("InitVideo: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("InitVideo: Exception {0}", e.Message));
            }
        }

        private async void UninitVideo()
        {
            try
            {
                OperationResult result = await MS2TesterHelper.UninitVideo();
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("UninitVideo: success");
                }
                else
                {
                    Debug.WriteLine("UninitVideo: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("UninitVideo: Exception {0}", e.Message));
            }
        }

        private async Task<List<String>> GetVideoDevices()
        {
            List<String> result = null;
            try
            {
                result = await MS2TesterHelper.GetVideoDevices();
                if (result != null)
                {
                    Debug.WriteLine("GetVideoDevices: success");
                }
                else
                {
                    Debug.WriteLine("GetVideoDevices: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("GetVideoDevices: Exception {0}", e.Message));
            }
            return result;
        }

        private async void StartVideoStream(String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            try
            {
                _videoSource = new libmswinrtvid.SwapChainPanelSource();
                _videoSource.Start(VideoSwapChainPanel);
                _previewSource = new libmswinrtvid.SwapChainPanelSource();
                _previewSource.Start(PreviewSwapChainPanel);
                var vcc = VoipCallCoordinator.GetDefault();
                var entryPoint = typeof(PhoneCallTask).FullName;
                var status = await vcc.ReserveCallResourcesAsync(entryPoint);
                var capabilities = VoipPhoneCallMedia.Audio | VoipPhoneCallMedia.Video;
                call = vcc.RequestNewOutgoingCall("FooContext", "FooContact", "MS2Tester", capabilities);
                call.NotifyCallActive();
                OperationResult result = await MS2TesterHelper.StartVideoStream(VideoSwapChainPanel.Name, PreviewSwapChainPanel.Name, camera, codec, videoSize, frameRate, bitRate);
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("StartVideoStream: success");
                }
                else
                {
                    Debug.WriteLine("StartVideoStream: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("StartVideoStream: Exception {0}", e.Message));
            }
        }

        private async void StopVideoStream()
        {
            try
            {
                OperationResult result = await MS2TesterHelper.StopVideoStream();
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("StopVideoStream: success");
                }
                else
                {
                    Debug.WriteLine("StopVideoStream: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("StopVideoStream: Exception {0}", e.Message));
            }
            call.NotifyCallEnded();
            call = null;
        }

        private async void ChangeCamera(String camera)
        {
            try
            {
                OperationResult result = await MS2TesterHelper.ChangeCamera(camera);
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("ChangeCamera: success");
                }
                else
                {
                    Debug.WriteLine("ChangeCamera: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("ChangeCamera: Exception {0}", e.Message));
            }
        }

        private async Task<int> GetOrientation()
        {
            int result = 0;
            try
            {
                result = await MS2TesterHelper.GetOrientation();
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("GetVideoDevices: Exception {0}", e.Message));
            }
            return result;
        }

        private async void SetOrientation(int degrees)
        {
            try
            {
                OperationResult result = await MS2TesterHelper.SetOrientation(degrees);
                if (result == OperationResult.Succeeded)
                {
                    Debug.WriteLine("SetOrientation: success");
                }
                else
                {
                    Debug.WriteLine("SetOrientation: failure");
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("SetOrientation: Exception {0}", e.Message));
            }
        }

        private void ChangeCameraButton_Click(object sender, RoutedEventArgs e)
        {
            String camera = (CameraComboBox.SelectedItem as ComboBoxItem).Content as String;
            ChangeCamera(camera);
        }

        private async void SetVideoOrientation()
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

            int currentDegrees = await GetOrientation();
            if (currentDegrees != degrees)
            {
                SetOrientation(degrees);
            }
        }

        private void AdaptVideoSize()
        {
            if (ActualWidth > 640)
            {
                VideoGrid.Width = 640;
            }
            else
            {
                VideoGrid.Width = ActualWidth;
            }
            VideoGrid.Height = VideoGrid.Width * 3 / 4;
            PreviewSwapChainPanel.Width = VideoGrid.Width / 4;
            PreviewSwapChainPanel.Height = VideoGrid.Height / 4;
        }


        private VoipPhoneCall call;
        private ApplicationViewOrientation displayOrientation;
        private DisplayInformation displayInformation;
        private SimpleOrientationSensor orientationSensor;
        private SimpleOrientation deviceOrientation;
        private libmswinrtvid.SwapChainPanelSource _videoSource;
        private libmswinrtvid.SwapChainPanelSource _previewSource;
    }
}
