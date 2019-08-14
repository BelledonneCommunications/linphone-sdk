/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
﻿#pragma once

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2_tester.h"

namespace BelledonneCommunications
{
	namespace Mediastreamer2
	{
		namespace Tester
		{
			public interface class OutputTraceListener
			{
			public:
				void outputTrace(Platform::String^ lev, Platform::String^ msg);
			};

			public ref class NativeTester sealed
			{
			public:
				void setOutputTraceListener(OutputTraceListener^ traceListener);
				unsigned int nbTestSuites();
				unsigned int nbTests(Platform::String^ suiteName);
				Platform::String^ testSuiteName(int index);
				Platform::String^ testName(Platform::String^ suiteName, int testIndex);
				void initialize(Windows::Storage::StorageFolder^ writableDirectory, Platform::Boolean ui);
				bool run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);
				void runAllToXml();
				void initVideo();
				void uninitVideo();
				void startVideoStream(Platform::String^ videoSwapChainPanelName, Platform::String^ previewSwapChainPanelName, Platform::String^ camera, Platform::String^ codec, Platform::String^ videoSize, unsigned int frameRate, unsigned int bitRate, Platform::Boolean usePreviewStream);
				void stopVideoStream();
				int getOrientation() { return _deviceRotation; }
				void setOrientation(int degrees);
				void changeCamera(Platform::String^ camera);

				static property NativeTester^ Instance
				{
					NativeTester^ get() { return _instance; }
				}
				property Windows::Foundation::IAsyncAction^ AsyncAction
				{
					Windows::Foundation::IAsyncAction^ get() { return _asyncAction; }
				}
				property Windows::Foundation::Collections::IVector<Platform::String^>^ VideoDevices
				{
					Windows::Foundation::Collections::IVector<Platform::String^>^ get();
				}
			private:
				NativeTester();
				~NativeTester();
				void initMS2();
				void uninitMS2();

				static NativeTester^ _instance;
				Windows::Foundation::IAsyncAction^ _asyncAction;
				MSFactory *_factory;
				VideoStream *_videoStream;
				int _deviceRotation;
				Platform::Boolean _usePreviewStream;
			};
		}
	}
}
