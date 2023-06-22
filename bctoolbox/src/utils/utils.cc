/*
 * Copyright (c) 2016-2021 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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

#include "bctoolbox/utils.hh"
#include <sstream>

using namespace std;

#ifdef BCTBX_WINDOWS_UWP
using namespace Windows::System;
using namespace Windows::Foundation;
#endif

vector<string> bctoolbox::Utils::split(const string &str, const string &delimiter) {
	vector<string> out;

	size_t pos = 0, oldPos = 0;
	for (; (pos = str.find(delimiter, pos)) != string::npos; oldPos = pos + delimiter.length(), pos = oldPos)
		out.push_back(str.substr(oldPos, pos - oldPos));
	out.push_back(str.substr(oldPos));

	return out;
}

string bctoolbox::Utils::fold(const string &str) {
	string output = str;
	size_t crlf = 0;
	size_t next_crlf = 0;
	const char *endline = "\r\n";

	while (next_crlf != string::npos) {
		next_crlf = output.find(endline, crlf);
		if (next_crlf != string::npos) {
			if (next_crlf - crlf > 75) {
				output.insert(crlf + 74, "\r\n ");
				crlf += 76;
			} else {
				crlf = next_crlf + 2;
			}
		}
	}

	return output;
}

string bctoolbox::Utils::unfold(const string &str) {
	string output = str;
	const char *endline = "\r\n";
	size_t crlf = output.find(endline);

	if (crlf == string::npos) {
		endline = "\n";
		crlf = output.find(endline);
	}

	while (crlf != string::npos) {
		if (isspace(output[crlf + strlen(endline)])) {
			output.erase(crlf, strlen(endline) + 1);
		} else {
			crlf += strlen(endline);
		}

		crlf = output.find(endline, crlf);
	}

	return output;
}

void bctoolbox::Utils::replace(string &source, const string &from, const string &to, const bool recursive) {
	size_t start_pos = 0;
	while ((start_pos = source.find(from, start_pos)) != string::npos) {
		source.replace(start_pos, from.length(), to);
		if (recursive) start_pos += to.length();
	}
}

std::string bctoolbox::Utils::getMemoryReportAsString() {
	std::ostringstream ossReport;
#ifdef WIN32
	// Use to convert bytes to MB
	const int division = 1048576;
	MEMORYSTATUSEX memoryStatus;
	memoryStatus.dwLength = sizeof(memoryStatus);
	GlobalMemoryStatusEx(&memoryStatus);
	ossReport << "Memory stats (MB): Usage=" << memoryStatus.dwMemoryLoad
	          << ", Total physical=" << memoryStatus.ullTotalPhys / division
	          << ", Free physical=" << memoryStatus.ullAvailPhys / division
	          << ", Total paging file=" << memoryStatus.ullTotalPageFile / division
	          << ", Free paging file=" << memoryStatus.ullAvailPageFile / division
	          << ", Total virtual=" << memoryStatus.ullTotalVirtual / division
	          << ", Free virtual=" << memoryStatus.ullAvailVirtual / division
	          << ", Free extended=" << memoryStatus.ullAvailExtendedVirtual / division;
#ifdef BCTBX_WINDOWS_UWP
	ossReport << " | UWP App Memory (MB): Usage=" << MemoryManager::AppMemoryUsage / division
	          << ", Usage limit=" << MemoryManager::AppMemoryUsageLimit / division
	          << ", Usage level=" << (int)MemoryManager::AppMemoryUsageLevel
	          << ", Expected usage limit=" << MemoryManager::ExpectedAppMemoryUsageLimit / division
	          << ", Free=" << (long)(MemoryManager::AppMemoryUsageLimit - MemoryManager::AppMemoryUsage) / division;
#endif
#endif
	return ossReport.str();
}

bool bctoolbox::Utils::isExecutableInstalled(const std::string &executable, const std::string &resource) {
	auto pos = executable.find_last_of('/');
	if (pos == std::string::npos) {
		pos = executable.find_last_of('\\');
	}

	std::string path = executable.substr(0, pos + 1).append("../share/");
	std::string name = executable.substr(pos + 1);

	std::string check = path + name + "/" + resource;

	return bctbx_file_exist(check.c_str()) == 0;
}
