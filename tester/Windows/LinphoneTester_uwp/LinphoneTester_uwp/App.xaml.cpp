//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace LinphoneTester_uwp;

using namespace Platform;
using namespace Concurrency;
using namespace System::Threading::Core;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::System;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

using namespace BelledonneCommunications::Linphone::Tester;

#include <stdlib.h>
#include <iostream>

#include <ppltasks.h>
#include <sstream>

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
	if(__argc == 1){// Need at least one arguments to avoid starting senseless processes
		mQuitApp = true;
	}else{
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
				if (s_str == "--verbose")
					mVerbose = true;
				mArgs[i]  = ref new Platform::String(w_char);
			}
		}
		if( mDelegate)
			StartNewProcess();
	}
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
	if(mQuitApp)
		CoreApplication::Exit();
	else if(!mDelegate){
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
			auto space = ref new Platform::String(L" ");
			auto quote = ref new Platform::String(L"\"");
			auto request = ref new Platform::String(L"");
			for(unsigned int i = 0 ; i < mArgs->Length ; ++i){
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
			IBuffer^ buffer = ref new Buffer(255);

			Concurrency::create_task(mStreamSocket->InputStream->ReadAsync(buffer, 255, InputStreamOptions::Partial)).then([=](IBuffer^ receivedBuffer){
				CoreApplication::Exit();
				});
			dataWriter->WriteString(request);
			Concurrency::create_task(dataWriter->StoreAsync()).then([=](Concurrency::task< unsigned int >){
			});
		});
	
	}catch (Platform::Exception^ ex)
        {
            Windows::Networking::Sockets::SocketErrorStatus webErrorStatus = Windows::Networking::Sockets::SocketError::GetStatus(ex->HResult);
            wprintf(L"[%d] %s, %s", webErrorStatus, webErrorStatus.ToString()->Data(), ex->Message->Data());
        }
}
void App::OnActivated(IActivatedEventArgs^ e)
{
	if(mQuitApp)
		CoreApplication::Exit();
	else if(!mDelegate){
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
	SetCurrentDirectory (Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	NativeTester::Instance->initialize(mArgs, ApplicationData::Current->LocalFolder, mSpecificTest, mVerbose);
	NativeTester::Instance->runAllToXml();
	return  task_from_result();
}

task<void> waitForTests(){
	Concurrency::create_task(NativeTester::Instance->AsyncAction).wait();
	return  task_from_result();
}

void App::startTests(){
	Concurrency::create_task(InitializeTests()).then([](){
		return waitForTests();
		}).then([this]{
			if(mIsExitOnEnd) 
				CoreApplication::Exit();
		});
}