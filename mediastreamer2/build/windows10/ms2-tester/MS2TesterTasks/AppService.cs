using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;
using Windows.Foundation.Collections;

namespace MS2TesterTasks
{
    public sealed class AppService : IBackgroundTask
    {
        public void Run(IBackgroundTaskInstance taskInstance)
        {
            AppServiceTriggerDetails triggerDetail = taskInstance.TriggerDetails as AppServiceTriggerDetails;
            _deferral = taskInstance.GetDeferral();

            // Register for Task Cancel callback
            taskInstance.Canceled += TaskInstance_Canceled;

            AppServiceConnection connection = triggerDetail.AppServiceConnection;
            _connection = connection;
            connection.RequestReceived += Connection_RequestReceived;
        }

        private async void Connection_RequestReceived(AppServiceConnection sender, AppServiceRequestReceivedEventArgs args)
        {
            var deferral = args.GetDeferral();
            var response = new ValueSet();
            response[BackgroundOperation.Result] = (int)OperationResult.Failed;
            bool stop = false;
            try
            {
                var message = args.Request.Message;
                if (message.ContainsKey(BackgroundOperation.NewBackgroundRequest))
                {
                    switch ((BackgroundRequest)message[BackgroundOperation.NewBackgroundRequest])
                    {
                        case BackgroundRequest.InitVideo:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.InitVideo;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.InitVideo();
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.UninitVideo:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.UninitVideo;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.UninitVideo();
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.GetVideoDevices:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.GetVideoDevices;
                            CurrentOperation.AppRequestDeferral = deferral;
                            response[BackgroundOperation.ReturnValue] = CurrentOperation.GetVideoDevices().ToArray();
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.StartVideoStream:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.StartVideoStream;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.StartVideoStream(
                                message[StartVideoStreamArguments.VideoSwapChainPanelName.ToString()] as String,
                                message[StartVideoStreamArguments.PreviewSwapChainPanelName.ToString()] as String,
                                message[StartVideoStreamArguments.Camera.ToString()] as String,
                                message[StartVideoStreamArguments.Codec.ToString()] as String,
                                message[StartVideoStreamArguments.VideoSize.ToString()] as String,
                                Convert.ToUInt32(message[StartVideoStreamArguments.FrameRate.ToString()]),
                                Convert.ToUInt32(message[StartVideoStreamArguments.BitRate.ToString()]));
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.StopVideoStream:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.StopVideoStream;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.StopVideoStream();
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.ChangeCamera:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.ChangeCamera;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.ChangeCamera(message[ChangeCameraArguments.Camera.ToString()] as String);
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.GetOrientation:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.GetOrientation;
                            CurrentOperation.AppRequestDeferral = deferral;
                            response[BackgroundOperation.ReturnValue] = CurrentOperation.GetOrientation();
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        case BackgroundRequest.SetOrientation:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.SetOrientation;
                            CurrentOperation.AppRequestDeferral = deferral;
                            CurrentOperation.SetOrientation(Convert.ToInt32(message[SetOrientationArguments.Degrees.ToString()]));
                            response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                            break;
                        default:
                            stop = true;
                            break;
                    }
                }
            }
            finally
            {
                if (stop)
                {
                    _deferral.Complete();
                } else
                {
                    await args.Request.SendResponseAsync(response);
                }
            }
        }

        private void TaskInstance_Canceled(IBackgroundTaskInstance sender, BackgroundTaskCancellationReason reason)
        {
            if (_deferral != null)
            {
                _deferral.Complete();
            }
        }

        private AppServiceConnection _connection;
        private BackgroundTaskDeferral _deferral;
    }
}
