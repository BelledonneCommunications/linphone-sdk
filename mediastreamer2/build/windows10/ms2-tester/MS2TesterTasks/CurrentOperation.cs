using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.AppService;

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

        public static AppServiceDeferral AppRequestDeferal
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


        public static void StartVideoStream(String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            lock (_lock)
            {
                ms2_tester_runtime_component.MS2Tester.Instance.startVideoStream(null, null, camera, codec, videoSize, frameRate, bitRate);
            }
        }


        private static Object _lock = new Object();
        private static AppServiceRequest _appRequest = null;
        private static AppServiceDeferral _appDeferral = null;
        private static BackgroundRequest _request = BackgroundRequest.InValid;
    }
}
