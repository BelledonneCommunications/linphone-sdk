using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;

namespace InvokeServer
{
    class Program
    {
        public static void Main()
        {
            IPAddress localIP = IPAddress.Parse("127.0.0.1");
            int portNo = 50000;

            Console.WriteLine("---- Listener ----");
            Console.WriteLine("Local IP = {0}", localIP);
            Console.WriteLine("Port No. = {0}", portNo);

            IPEndPoint ep = new IPEndPoint(localIP, portNo);

            Socket listener = new Socket(AddressFamily.InterNetwork,
                                         SocketType.Stream,
                                         ProtocolType.Tcp);
            listener.Bind(ep);
            listener.Listen(1);
            //int count = 0;
            while (true)
            {
                Console.WriteLine("waiting..");
                Socket connection = listener.Accept();

                Console.WriteLine("Accepted!!\n");
                IPEndPoint remoteEP = (IPEndPoint)connection.RemoteEndPoint;
                Console.WriteLine("Remote IP = {0}", remoteEP.Address);
                Console.WriteLine("Remote Port = {0}", remoteEP.Port);

                Console.WriteLine("receiving data...");
                byte[] receiveData = new byte[1000];
                int size = connection.Receive(receiveData);
                string receiveString = Encoding.UTF8.GetString(receiveData, 0, size);
                Console.WriteLine("received!");
                Console.WriteLine("raceived data = {0}\n", receiveString);

                //var p = System.Diagnostics.Process.Start("LinphoneTester_uwp.exe","--suite \"Register\"");
                //if(count++ >=0){// DEBUG SWITCH
                    /*
                    ProcessStartInfo _processStartInfo = new ProcessStartInfo();
                    _processStartInfo.WorkingDirectory = @"C:\\projects\\test\\3\\LinphoneTester_uwp\\x64\\Debug\\LinphoneTester_uwp\\AppX";
                    _processStartInfo.FileName = @"LinphoneTester_uwp.exe";
                    _processStartInfo.Arguments = "--xml --suite \"Video Call quality\"";
                    //_processStartInfo.CreateNoWindow = true;
                    Process myProcess = Process.Start(_processStartInfo);*/
                    var parsedCommand = receiveString.Split(' ');
                    var parameters = string.Join(" ", parsedCommand.Skip(1));
                    var process = System.Diagnostics.Process.Start(parsedCommand[0], parameters);
                    process.PriorityClass = ProcessPriorityClass.BelowNormal;// Avoid freezing OS
                    process.EnableRaisingEvents = true;
                    process.Exited += (sender, e) => {
                        Debug.WriteLine("Process exited with exit code " + process.ExitCode.ToString());
                        byte[] sendData = new byte[1];
                        try{
                            connection.Send(sendData);
                            connection.Close();
                        }catch
                        {
                        }
                    };

                //}

                //byte[] sendData = Encoding.UTF8.GetBytes("Ok\n");
                //byte[] sendData = new byte[1];
                //connection.Send(sendData);
            }
        }
    }
}
