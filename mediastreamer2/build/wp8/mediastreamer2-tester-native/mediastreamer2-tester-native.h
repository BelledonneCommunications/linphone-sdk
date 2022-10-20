/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
﻿#pragma once

#include "mediastreamer2_tester.h"

namespace mediastreamer2_tester_native
{
	enum OutputTraceLevel {
		Debug,
		Message,
		Warning,
		Error,
		Raw
	};

	public interface class OutputTraceListener
	{
	public:
		void outputTrace(int level, Platform::String^ msg);
	};

    public ref class Mediastreamer2TesterNative sealed
    {
    public:
        Mediastreamer2TesterNative();
		virtual ~Mediastreamer2TesterNative();
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		unsigned int nbTests(Platform::String^ suiteName);
		Platform::String^ testSuiteName(int index);
		Platform::String^ testName(Platform::String^ suiteName, int testIndex);
		void run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);
    };
}
