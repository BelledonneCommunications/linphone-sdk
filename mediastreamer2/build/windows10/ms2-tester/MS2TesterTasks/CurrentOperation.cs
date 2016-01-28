using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;

namespace MS2TesterTasks
{
    static class CurrentOperation
    {
        public static AppServiceRequest AppRequest
        {
            set
            {
                lock (_lock)
                {
                    _appRequest = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _appRequest;
                }
            }
        }

        public static BackgroundRequest Request
        {
            set
            {
                lock (_lock)
                {
                    _request = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _request;
                }
            }
        }

        public static AppServiceDeferral AppRequestDeferral
        {
            set
            {
                lock (_lock)
                {
                    _appDeferral = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _appDeferral;
                }
            }
        }

        public static BackgroundTaskDeferral PhoneCallTaskDeferral
        {
            set
            {
                lock (_lock)
                {
                    _phoneCallTaskDeferral = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _phoneCallTaskDeferral;
                }
            }
        }


        public static void InitVideo()
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.initVideo();
            }
        }

        public static void UninitVideo()
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.uninitVideo();
            }
        }

        public static List<String> GetVideoDevices()
        {
            lock (_lock)
            {
                return ms2_tester_runtime_component.MS2Tester.Instance.VideoDevices.ToList<String>();
            }
        }

        public static void StartVideoStream(String videoSwapChainPanelName, String previewSwapChainPanelName, String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.startVideoStream(videoSwapChainPanelName, previewSwapChainPanelName, camera, codec, videoSize, frameRate, bitRate);
            }
        }

        public static void StopVideoStream()
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.stopVideoStream();
            }
        }

        public static void ChangeCamera(String camera)
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.changeCamera(camera);
            }
        }

        public static int GetOrientation()
        {
            lock (_lock)
            {
                return ms2_tester_runtime_component.MS2Tester.Instance.getOrientation();
            }
        }

        public static void SetOrientation(int degrees)
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.setOrientation(degrees);
            }
        }


        private static Object _lock = new Object();
        private static AppServiceRequest _appRequest = null;
        private static AppServiceDeferral _appDeferral = null;
        private static BackgroundTaskDeferral _phoneCallTaskDeferral = null;
        private static BackgroundRequest _request = BackgroundRequest.InValid;
    }
}
