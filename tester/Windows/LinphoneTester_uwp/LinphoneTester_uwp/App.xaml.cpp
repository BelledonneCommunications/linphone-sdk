//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace LinphoneTester_uwp;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

using namespace Concurrency;
using namespace Windows::ApplicationModel::Core;

using namespace Windows::Storage;
using namespace BelledonneCommunications::Linphone::Tester;

using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Core;
using namespace Windows::System::Threading;
using namespace Windows::System;

#include <stdlib.h>
#include <iostream>

#include <ppltasks.h>
#include <sstream>

using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Navigation;

using namespace System::Threading::Core;

task<void> init(){
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
    ref new Windows::UI::Core::DispatchedHandler ([]
	{
		Window::Current->Activate();  
	}));
	return  task_from_result();

}

App::App()
{

    InitializeComponent();
	CoreApplication::EnablePrelaunch(false);
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	mArgs = ref new Platform::Array<Platform::String^>(__argc);
	if (! AttachConsole(ATTACH_PARENT_PROCESS))   // try to hijack existing console of command line
		AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	wprintf(L"Parsed command-line arguments:\n");
    for (int i = 0; i < __argc; i++)
    {
		wprintf(L"__argv[%d] = %S\n", i, __argv[i]);
		std::string s_str;
		if( i == 0){
			char fileName[255], extension[10];
			_splitpath_s(__argv[0], NULL, 0, NULL, 0, fileName, 255, extension, 10);
			s_str = std::string(fileName)+std::string(extension);
		}else{
			s_str = std::string(__argv[i]);
		}
		std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
		const wchar_t* w_char = wid_str.c_str();
		if( s_str == "--keep"){
			mIsExitOnEnd = false;
			mArgs[i] = "";
		}else if( s_str == "--child"){
			mDelegate = true;
			mArgs[i] = "";
		}else{
			if( s_str == "--suite" || s_str == "--test")
				mSpecificTest = true;
			mArgs[i]  = ref new Platform::String(w_char);
		}
    }
	if( mDelegate)
		StartNewProcess();
	/*
	if(!mDelegate){
			
	   //auto file = Windows::ApplicationModel::Package::Current->InstalledLocation->GetFileAsync("LinphoneTester_uwp.exe --child");

		//Windows::System::Launcher::LaunchFileAsync(file);
		//system("LinphoneTester_uwp.exe --child");
		//if (! AttachConsole(ATTACH_PARENT_PROCESS))   // try to hijack existing console of command line
//			AllocConsole();
	}else
		StartNewProcess();
		*/
	//if (! AttachConsole(ATTACH_PARENT_PROCESS))   // try to hijack existing console of command line
      //  AllocConsole();   
	//std::cout << "TOTOT" << std::endl;
	//if(!mIsMain)
			//CoreApplication::Exit();
	/*
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
    ref new Windows::UI::Core::DispatchedHandler ([]
	{
		Window::Current->Activate();  
	}));

	if(mIsMain){
		if (! AttachConsole(ATTACH_PARENT_PROCESS))   // try to hijack existing console of command line
        AllocConsole();                           // or create your own.
	}else
	{	
		Concurrency::create_task(init()).get();
		
	}
		*/
    // Keep the console window alive in case you want to see console output when running from within Visual Studio
      
		
	/*

	   NativeTester::Instance->initialize(args, ApplicationData::Current->LocalFolder, specificTest, false);// when ui is true, we don't sotre in xml (that means that test is run only for selected test)
       NativeTester::Instance->runAllToXml();

	   create_task(NativeTester::Instance->AsyncAction).then([isMain]{
				if(isMain){
					wprintf(L"Press 'Enter' to continue: ");
					getchar();
				}
                CoreApplication::Exit();
		   });*/
	
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
	if(!mDelegate){
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

    // Do not repeat app initialization when the Window already has content,
    // just ensure that the window is active
    if (rootFrame == nullptr)
    {
        rootFrame = ref new Frame();
        rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);
        if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
        {
            // TODO: Restore the saved session state only when appropriate, scheduling the
            // final launch steps after the restore is complete
        }
        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Place the frame in the current Window
            Window::Current->Content = rootFrame;
            // Ensure the current window is active
            Window::Current->Activate();
        }
    }
    else
    {
        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Ensure the current window is active
            Window::Current->Activate();
        }
    }
	startTests();
	}
}

void App::StartNewProcess(){
    mStreamSocket = ref new Windows::Networking::Sockets::StreamSocket();
    auto hostName = ref new Windows::Networking::HostName(L"localhost");
	try{
    Concurrency::create_task(mStreamSocket->ConnectAsync(hostName, L"50000")).then([=](Concurrency::task< void >){
		auto dataWriter = ref new DataWriter(mStreamSocket->OutputStream);
		//auto dataReader = ref new DataReader(mStreamSocket->InputStream);
		//dataReader->InputStreamOptions = InputStreamOptions::Partial;
		auto space = ref new Platform::String(L" ");
		auto quote = ref new Platform::String(L"\"");
		auto request = ref new Platform::String(L"");
		for(int i = 0 ; i < mArgs->Length ; ++i){
			if(mArgs[i] != ""){
				if(request != "")// Remove first space
					request = Platform::String::Concat(request, space);
				std::wstring wid_str = mArgs[i]->Data();
				if( wid_str.find_first_of(' ', 0) !=  std::string::npos){// Add missing quotes as it was removed in arguments
					request = Platform::String::Concat(request, quote );
					request = Platform::String::Concat(request, mArgs[i]);
					request = Platform::String::Concat(request, quote);
				}else
					request = Platform::String::Concat(request, mArgs[i]);
			}
		}

		//dataWriter->WriteUInt32(request->Length());
		
		
		IBuffer^ buffer = ref new Buffer(255);

		Concurrency::create_task(mStreamSocket->InputStream->ReadAsync(buffer, 255, InputStreamOptions::Partial)).then([=](IBuffer^ receivedBuffer){
			CoreApplication::Exit();
			});

		/*
		Concurrency::create_task(dataReader->LoadAsync(255)).then(
                [=](unsigned int bytesLoaded)
        {
				 wprintf(L"Parsed command-line arguments:\n");
		});*/
		/*
		ThreadPoolTimer ^ PeriodicTimer = ThreadPoolTimer::CreatePeriodicTimer(
        ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
        {

				 wprintf(L"Parsed command-line arguments:%d\n", this->mStreamSocket->);
        }), 100);*/
//		}catch(...){

//		}
//-----


		dataWriter->WriteString(request);
		Concurrency::create_task(dataWriter->StoreAsync()).then([=](Concurrency::task< unsigned int >){
		});
	});
		
		/*.then([=](Concurrency::task< void >){
		streamSocket->
		});*/
	
	}catch (Platform::Exception^ ex)
        {
            Windows::Networking::Sockets::SocketErrorStatus webErrorStatus = Windows::Networking::Sockets::SocketError::GetStatus(ex->HResult);
            wprintf(L"%[d] %s, %s", webErrorStatus, webErrorStatus.ToString()->Data(), ex->Message->Data());
        }
}
void App::OnActivated(IActivatedEventArgs^ e)
{
	if(!mDelegate){
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);
    if (rootFrame == nullptr)
    {
        rootFrame = ref new Frame();
        rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);
        if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
        {
        }
    }
	if (rootFrame->Content == nullptr)
    {
        rootFrame->Navigate(TypeName(MainPage::typeid),nullptr);// e->Arguments);
    }
    Window::Current->Content = rootFrame;
    Window::Current->Activate();
	startTests();
	}
}

void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
    (void) sender;  // Unused parameter
    (void) e;   // Unused parameter
}

void App::OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e)
{
    throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}
task<void> App::InitializeTests(){
	auto a = ApplicationData::Current->LocalCacheFolder->Path;
	auto b = ApplicationData::Current->LocalFolder->Path;
	auto c = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;	
	auto d = ApplicationData::Current->RoamingFolder->Path;
	auto f = ApplicationData::Current->TemporaryFolder->Path;
	
	SetCurrentDirectory (c->Data());

	NativeTester::Instance->initialize(mArgs, ApplicationData::Current->LocalFolder, mSpecificTest, false);// when ui is true, we don't sotre in xml (that means that test is run only for selected test)
	NativeTester::Instance->runAllToXml();
	return  task_from_result();
}
task<void> waitForTests(){
	Concurrency::create_task(NativeTester::Instance->AsyncAction).wait();
	return  task_from_result();
}
void App::startTests(){
	//auto worker = ref new WorkItemHandler([this](IAsyncAction ^workItem) {
		//});
	
	Concurrency::create_task(InitializeTests()).then([](){
		return waitForTests();
		}).then([this]{if(mIsExitOnEnd) CoreApplication::Exit();});
		

	/*
		auto worker = ref new WorkItemHandler([this](IAsyncAction ^workItem) {
		  
		});
		Concurrency::create_task(ThreadPool::RunAsync(worker)).then([]{
		   //return NativeTester::Instance->AsyncAction;
			return t();
		}).then([this]{
			//while(1){}
			//if(mIsMain){
			//	wprintf(L"Press 'Enter' to continue: ");
			//	getchar();
			//}else
			//	CoreApplication::Exit();
		});
		*/
}
/*
using namespace System;
int __cdecl main(::Platform::Array<::Platform::String^>^ args)
{
    (void)args; // Unused parameter
    ::Windows::UI::Xaml::Application::Start(ref new ::Windows::UI::Xaml::ApplicationInitializationCallback(
        [](::Windows::UI::Xaml::ApplicationInitializationCallbackParams^ p) {
            (void)p; // Unused parameter
            auto app = ref new ::LinphoneTester_uwp::App();
        }));
}*/
