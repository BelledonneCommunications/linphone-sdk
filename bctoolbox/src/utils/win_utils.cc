/*
 * Copyright (c) 2022 Belledonne Communications SARL.
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
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"

#include <dbghelp.h>
#include <eh.h>
#include <intrin.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>

#include <errhandlingapi.h>

#pragma comment(lib, "dbghelp.lib")

using namespace std;

//-------------------------------------------------
//----------------   PLATFORM ---------------------
//-------------------------------------------------
#ifdef BCTBX_WINDOWS_UWP

std::string GetBackTrace(int SkipFrames) {
	std::stringstream result;
	constexpr unsigned int TRACE_MAX_STACK_FRAMES = 99;
	void *stack[TRACE_MAX_STACK_FRAMES];
	ULONG hash;
	const int numFrames = CaptureStackBackTrace(SkipFrames + 1, TRACE_MAX_STACK_FRAMES, stack, &hash);
	constexpr auto MODULE_BUF_SIZE = 4096U;
	char modulePath[MODULE_BUF_SIZE];
	result << "Stack hash: " << hash << "\n";
	for (int i = 0; i < numFrames; ++i) {
		void *moduleBaseVoid = nullptr;
		RtlPcToFileHeader(stack[i], &moduleBaseVoid);
		auto moduleBase = (const unsigned char *)moduleBaseVoid;
		const char *moduleFilename = modulePath;
		result << i << ": ";
		if (moduleBase != nullptr) {
			GetModuleFileNameA((HMODULE)moduleBase, modulePath, MODULE_BUF_SIZE);
			result << moduleFilename << "+" << (uint32_t)((unsigned char *)stack[i] - moduleBase) << "\n";
		} else result << moduleFilename << "+" << (uint64_t)stack[i] << "\n";
	}
	return result.str();
}

// Hooks
void _signal_hook(int u) {
	// Skip 8 useless stack frames
	bctbx_error("UWP Stack trace %d :\n%s\n", u, GetBackTrace(8).c_str());
}

//------------------------------------

#else

struct StackFrame {
	DWORD64 address;
	std::string name;
	std::string module;
	unsigned int line;
	std::string file;
};

std::vector<StackFrame> getStackTrace() {
#if _WIN64
	DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
	DWORD machine = IMAGE_FILE_MACHINE_I386;
#endif
	HANDLE process = GetCurrentProcess();
	HANDLE thread = GetCurrentThread();

	if (SymInitialize(process, NULL, TRUE) == FALSE) {
		bctbx_error(__FUNCTION__ ": Failed to call SymInitialize.");
		return std::vector<StackFrame>();
	}

	SymSetOptions(SYMOPT_LOAD_LINES);

	CONTEXT context = {};
	context.ContextFlags = CONTEXT_FULL;
	RtlCaptureContext(&context);

#if _WIN64
	STACKFRAME frame = {};
	frame.AddrPC.Offset = context.Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Rsp;
	frame.AddrStack.Mode = AddrModeFlat;
#else
	STACKFRAME frame = {};
	frame.AddrPC.Offset = context.Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Esp;
	frame.AddrStack.Mode = AddrModeFlat;
#endif
	bool first = true;

	std::vector<StackFrame> frames;
	while (
	    StackWalk(machine, process, thread, &frame, &context, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL)) {
		StackFrame f = {};
		f.address = frame.AddrPC.Offset;

#if _WIN64
		DWORD64 moduleBase = 0;
#else
		DWORD moduleBase = 0;
#endif

		moduleBase = SymGetModuleBase(process, frame.AddrPC.Offset);

		char moduelBuff[MAX_PATH];
		if (moduleBase && GetModuleFileNameA((HINSTANCE)moduleBase, moduelBuff, MAX_PATH)) {
			f.module = bctbx_basename(moduelBuff);
		} else {
			f.module = "Unknown Module";
		}
#if _WIN64
		DWORD64 offset = 0;
#else
		DWORD offset = 0;
#endif
		char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
		PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
		symbol->SizeOfStruct = (sizeof IMAGEHLP_SYMBOL) + 255;
		symbol->MaxNameLength = 254;

		if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &offset, symbol)) {
			f.name = symbol->Name;
		} else {
			f.name = "Unknown Name";
		}

		IMAGEHLP_LINE line;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

		DWORD offset_ln = 0;
		if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset_ln, &line)) {
			f.file = line.FileName;
			f.line = line.LineNumber;
		} else {
			f.line = -1;
		}

		if (!first) frames.push_back(f);
		first = false;
	}

	SymCleanup(process);

	return frames;
}

// Hooks
void _signal_hook(int u) {
	std::stringstream buff;
	buff << __FUNCTION__ << ":  General Fault: '" << std::to_string(u) << "'! \n";
	buff << "\n";

	std::vector<StackFrame> stack = getStackTrace();
	buff << "Callstack: \n";
	for (unsigned int i = 0; i < stack.size(); i++)
		buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << stack[i].line << ") in "
		     << stack[i].module << "\n";
	bctbx_error(buff.str().c_str());
}

#endif

//-------------------------------------------------
//-------------------------------------------------
//-------------------------------------------------

//---------------------------------

// Static old handlers : Use static and not capture in lambda because if a capture is present, the lambda cannot be
// converted to a function pointer and then, it is unusable by ahndlers
static LPTOP_LEVEL_EXCEPTION_FILTER gOldUnhandledException = NULL;
static std::terminate_handler gOldTerminateHandler = NULL;
static _crt_signal_t gOldABRT = NULL;
static _crt_signal_t gOldFPE = NULL;
static _crt_signal_t gOldILL = NULL;
static _crt_signal_t gOldEGV = NULL;
static _crt_signal_t gOldTERM = NULL;

// Generic Hooks

LONG WINAPI _unhandled_exceptions_hook(_EXCEPTION_POINTERS *pExp) {
	_signal_hook(SIGABRT);
	if (gOldUnhandledException) return gOldUnhandledException(pExp);
	else return EXCEPTION_EXECUTE_HANDLER;
}
void _abrt_signal_hook(int u) {
	_signal_hook(u);
	if (gOldABRT) gOldABRT(u);
}
void _fpe_signal_hook(int u) {
	_signal_hook(u);
	if (gOldFPE) gOldFPE(u);
}
void _ill_signal_hook(int u) {
	_signal_hook(u);
	if (gOldILL) gOldILL(u);
}
void _egv_signal_hook(int u) {
	_signal_hook(u);
	if (gOldEGV) gOldEGV(u);
}
void _term_signal_hook(int u) {
	_signal_hook(u);
	if (gOldTERM) gOldTERM(u);
}

void _terminate_hook() {
	_signal_hook(SIGTERM);
	if (gOldTerminateHandler) gOldTerminateHandler();
	else std::abort();
}

void setSignalHook(int sig, _crt_signal_t function, _crt_signal_t &oldFunction) {
	auto old = signal(sig, function);
	if (old != function) oldFunction = old;
}
void setTerminateHook(std::terminate_handler function, std::terminate_handler &oldFunction) {
	auto old = std::set_terminate(function);
	if (old != function) oldFunction = old;
}

void setUnhandledExceptionHook(LPTOP_LEVEL_EXCEPTION_FILTER function, LPTOP_LEVEL_EXCEPTION_FILTER &oldFunction) {
	auto old = SetUnhandledExceptionFilter(function);
	if (old != function) oldFunction = old;
}

void bctbx_set_stack_trace_hooks(bool_t use_bctbx_hooks) {
	// Try to take account of all handlers
	if (use_bctbx_hooks) {
		// Signal
		setSignalHook(SIGABRT, _abrt_signal_hook, gOldABRT); // Abnormal termination
		setSignalHook(SIGFPE, _fpe_signal_hook, gOldFPE);    // Floating-point error
		setSignalHook(SIGILL, _ill_signal_hook, gOldILL);    // Illegal instruction
		setSignalHook(SIGSEGV, _egv_signal_hook, gOldEGV);   // Illegal storage access
		setSignalHook(SIGTERM, _term_signal_hook, gOldTERM); // Terminate request
		                                                     // Termination
		setTerminateHook(_terminate_hook, gOldTerminateHandler);
		// Top level exception
		setUnhandledExceptionHook(_unhandled_exceptions_hook, gOldUnhandledException);
		// atexit(_test_atexit);// UWP doesn't catch it
	} else { // if 'old' is set, set 'old' in handlers and unset it to avoid overrides.
		if (gOldABRT) setSignalHook(SIGABRT, gOldABRT, gOldABRT); // Abnormal termination
		gOldABRT = NULL;
		if (gOldFPE) setSignalHook(SIGFPE, gOldFPE, gOldFPE); // Floating-point error
		gOldFPE = NULL;
		if (gOldILL) setSignalHook(SIGILL, gOldILL, gOldILL); // Illegal instruction
		gOldILL = NULL;
		if (gOldEGV) setSignalHook(SIGSEGV, gOldEGV, gOldEGV); // Illegal storage access
		gOldEGV = NULL;
		if (gOldTERM) setSignalHook(SIGTERM, gOldTERM, gOldTERM); // Terminate request
		gOldTERM = NULL;
		// Termination
		if (gOldTerminateHandler) setTerminateHook(gOldTerminateHandler, gOldTerminateHandler);
		gOldTerminateHandler = NULL;
		// Top level exception
		if (gOldUnhandledException) setUnhandledExceptionHook(gOldUnhandledException, gOldUnhandledException);
		gOldUnhandledException = NULL;
	}
}
