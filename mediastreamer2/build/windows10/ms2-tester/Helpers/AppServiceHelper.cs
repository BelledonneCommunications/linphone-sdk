using MS2TesterTasks;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.AppService;
using Windows.Foundation.Collections;

namespace ms2_tester.Helpers
{
    class AppServiceHelper
    {
        ~AppServiceHelper()
        {
            if (_appConnection != null)
            {
                _appConnection.Dispose();
                _appConnection = null;
            }
        }

        public async Task<ValueSet> SendMessageAsync(ValueSet message)
        {
            ValueSet returnValue = null;
            AppServiceConnection appConnection = await GetAppConnectionAsync();

            if (appConnection != null)
            {
                AppServiceResponse response = await appConnection.SendMessageAsync(message);

                if (response.Status == AppServiceResponseStatus.Success)
                {
                    if (response.Message.Keys.Contains(BackgroundOperation.Result))
                    {
                        returnValue = response.Message;
                    }
                }
            }

            return returnValue;
        }

        public async void SendMessage(ValueSet message)
        {
            AppServiceConnection appConnection = await GetAppConnectionAsync();

            if (appConnection != null)
            {
                await appConnection.SendMessageAsync(message);
            }
        }

        private async Task<AppServiceConnection> GetAppConnectionAsync()
        {
            AppServiceConnection appConnection = _appConnection;
            if (appConnection == null)
            {
                appConnection = new AppServiceConnection();
                appConnection.ServiceClosed += AppConnection_ServiceClosed;
                appConnection.AppServiceName = BackgroundOperation.AppServiceName;
                appConnection.PackageFamilyName = Windows.ApplicationModel.Package.Current.Id.FamilyName;

                AppServiceConnectionStatus status = await appConnection.OpenAsync();
                if (status == AppServiceConnectionStatus.Success)
                {
                    _appConnection = appConnection;
                }
            }
            return appConnection;
        }

        private void AppConnection_ServiceClosed(AppServiceConnection sender, AppServiceClosedEventArgs args)
        {
            _appConnection = null;
        }

        private AppServiceConnection _appConnection = null;
    }
}
