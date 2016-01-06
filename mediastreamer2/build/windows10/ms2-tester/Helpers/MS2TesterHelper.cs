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
        public static async Task<OperationResult> StartVideoStreamAsync(String camera, String codec, String videoSize, UInt32 frameRate, UInt32 bitRate)
        {
            AppServiceHelper appServiceHelper = new AppServiceHelper();

            ValueSet message = new ValueSet();
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
    }
}
