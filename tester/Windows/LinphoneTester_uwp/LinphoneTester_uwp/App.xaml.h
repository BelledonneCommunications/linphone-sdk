//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
using namespace Windows;
using namespace Windows::UI::Xaml;
using namespace Concurrency;

namespace LinphoneTester_uwp
{
	/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	protected:
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;
		virtual void OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ args) override;
	internal:
		App();
		void startTests();
		task<void> InitializeTests();
		void StartNewProcess();
	private:
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);
		bool mIsExitOnEnd = true;// Keep alive at the end
		bool mDelegate = false;// Delegate the test or do it in current instance
		bool mSpecificTest = false;
		bool mVerbose = false;
		bool mQuitApp = false;
		Platform::Array<Platform::String^>^ mArgs;
		Windows::Networking::Sockets::StreamSocket^ mStreamSocket;
	};
	
}
