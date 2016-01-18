using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MS2TesterTasks
{
    public enum BackgroundRequest
    {
        InitVideo,
        UninitVideo,
        GetVideoDevices,
        StartVideoStream,
        StopVideoStream,
        ChangeCamera,
        GetOrientation,
        SetOrientation,

        // Always keep this as the last option
        InValid
    }

    public enum StartVideoStreamArguments
    {
        VideoSwapChainPanelName,
        PreviewSwapChainPanelName,
        Camera,
        Codec,
        VideoSize,
        FrameRate,
        BitRate
    }

    public enum ChangeCameraArguments
    {
        Camera
    }

    public enum SetOrientationArguments
    {
        Degrees
    }

    public enum OperationResult
    {
        Succeeded,
        Failed
    }

    public static class BackgroundOperation
    {
        public static String AppServiceName
        {
            get { return _appServiceName; }
        }

        public static String NewBackgroundRequest
        {
            get { return _newBackgroundRequest; }
        }

        public static String Result
        {
            get { return _result; }
        }

        public static String ReturnValue
        {
            get { return _returnValue; }
        }

        const String _appServiceName = "MS2TesterTasks.AppService";
        const String _newBackgroundRequest = "NewBackgroundRequest";
        const String _result = "Result";
        const String _returnValue = "ReturnValue";
    }
}
