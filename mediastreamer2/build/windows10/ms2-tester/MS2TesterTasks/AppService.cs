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

        private void Connection_RequestReceived(AppServiceConnection sender, AppServiceRequestReceivedEventArgs args)
        {
            var deferral = args.GetDeferral();
            var response = new ValueSet();
            bool stop = false;
            try
            {
                var request = args.Request;
                var message = request.Message;
                if (message.ContainsKey(BackgroundOperation.NewBackgroundRequest))
                {
                    switch ((BackgroundRequest)message[BackgroundOperation.NewBackgroundRequest])
                    {
                        case BackgroundRequest.StartVideoStream:
                            CurrentOperation.AppRequest = args.Request;
                            CurrentOperation.Request = BackgroundRequest.StartVideoStream;
                            CurrentOperation.AppRequestDeferal = deferral;
                            CurrentOperation.StartVideoStream(
                                message[StartVideoStreamArguments.Camera.ToString()] as String,
                                message[StartVideoStreamArguments.Codec.ToString()] as String,
                                message[StartVideoStreamArguments.VideoSize.ToString()] as String,
                                Convert.ToUInt32(message[StartVideoStreamArguments.FrameRate.ToString()]),
                                Convert.ToUInt32(message[StartVideoStreamArguments.BitRate.ToString()]));
                            break;
                    }
                }
            }
            finally
            {

                if (stop)
                {
                    _deferral.Complete();
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
