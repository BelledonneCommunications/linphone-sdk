using MS2TesterTasks;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Collections;

namespace ms2_tester.Helpers
{
    class MS2TesterHelper
    {
        public static bool RunInBackground { get; set; }

        public static async Task<OperationResult> InitVideo()
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.InitVideo;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.initVideo();
                return OperationResult.Succeeded;
            }
        }

        public static async Task<OperationResult> UninitVideo()
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.UninitVideo;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.uninitVideo();
                return OperationResult.Succeeded;
            }
        }

        public static async Task<List<String>> GetVideoDevices()
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.GetVideoDevices;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if ((response != null) && ((OperationResult)(response[BackgroundOperation.Result]) == OperationResult.Succeeded))
                {
                    return ((IEnumerable<String>)response[BackgroundOperation.ReturnValue]).ToList<String>();
                }

                return null;
            }
            else
            {
                return ms2_tester_runtime_component.MS2Tester.Instance.VideoDevices.ToList();
            }
        }

        public static async Task<OperationResult> StartVideoStream(String videoSwapChainPanelName, String previewSwapChainPanelName, String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[StartVideoStreamArguments.VideoSwapChainPanelName.ToString()] = videoSwapChainPanelName;
                message[StartVideoStreamArguments.PreviewSwapChainPanelName.ToString()] = previewSwapChainPanelName;
                message[StartVideoStreamArguments.Camera.ToString()] = camera;
                message[StartVideoStreamArguments.Codec.ToString()] = codec;
                message[StartVideoStreamArguments.VideoSize.ToString()] = videoSize;
                message[StartVideoStreamArguments.FrameRate.ToString()] = frameRate;
                message[StartVideoStreamArguments.BitRate.ToString()] = bitRate;
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.StartVideoStream;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.startVideoStream(videoSwapChainPanelName, previewSwapChainPanelName, camera, codec, videoSize, frameRate, bitRate);
                return OperationResult.Succeeded;
            }
        }

        public static async Task<OperationResult> StopVideoStream()
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.StopVideoStream;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.stopVideoStream();
                return OperationResult.Succeeded;
            }
        }

        public static async Task<OperationResult> ChangeCamera(String camera)
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[ChangeCameraArguments.Camera.ToString()] = camera;
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.ChangeCamera;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.changeCamera(camera);
                return OperationResult.Succeeded;
            }
        }

        public static async Task<int> GetOrientation()
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.GetOrientation;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if ((response != null) && ((OperationResult)(response[BackgroundOperation.Result]) == OperationResult.Succeeded))
                {
                    return Convert.ToInt32(response[BackgroundOperation.ReturnValue]);
                }

                return 0;
            }
            else
            {
                return ms2_tester_runtime_component.MS2Tester.Instance.getOrientation();
            }
        }

        public static async Task<OperationResult> SetOrientation(int degrees)
        {
            if (RunInBackground)
            {
                AppServiceHelper appServiceHelper = new AppServiceHelper();

                ValueSet message = new ValueSet();
                message[SetOrientationArguments.Degrees.ToString()] = degrees;
                message[BackgroundOperation.NewBackgroundRequest] = (int)BackgroundRequest.SetOrientation;

                ValueSet response = await appServiceHelper.SendMessageAsync(message);
                if (response != null)
                {
                    return ((OperationResult)(response[BackgroundOperation.Result]));
                }

                return OperationResult.Failed;
            }
            else
            {
                ms2_tester_runtime_component.MS2Tester.Instance.setOrientation(degrees);
                return OperationResult.Succeeded;
            }
        }
    }
}
