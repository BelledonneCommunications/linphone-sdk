/*
* Copyright (c) 2010-2025 Belledonne Communications SARL.
*
* This file is part of linphone
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
// swiftlint:disable line_length

// Singleton instance that logs both info from the App and from the core, using the core log level. ([app] log_level parameter in linphonerc-factory-app

import UIKit
import os
import linphonesw

class Log: LoggingServiceDelegate {
	
	static let instance = Log()
	var service = LoggingService.Instance

	private init() {
		service.domain = Bundle.main.bundleIdentifier!
		LoggingService.Instance.addDelegate(delegate: self)
        setMask(verbose: true)
	}
	
    func setMask(verbose: Bool = false) {
		if verbose {
			LoggingService.Instance.logLevelMask = UInt(LogLevel.Fatal.rawValue + LogLevel.Error.rawValue + LogLevel.Warning.rawValue + LogLevel.Message.rawValue + LogLevel.Trace.rawValue + LogLevel.Debug.rawValue)
		} else {
			LoggingService.Instance.logLevelMask = UInt(LogLevel.Fatal.rawValue +  LogLevel.Error.rawValue +  LogLevel.Warning.rawValue)
		}
	}
	
	let levelToStrings: [Int: String] =
		[LogLevel.Debug.rawValue: "Debug"
		 , LogLevel.Trace.rawValue: "Trace"
		 , LogLevel.Message.rawValue: "Message"
		 , LogLevel.Warning.rawValue: "Warning"
		 , LogLevel.Error.rawValue: "Error"
		 , LogLevel.Fatal.rawValue: "Fatal"]
	
	let levelToOSleLogLevel: [Int: OSLogType] =
		[LogLevel.Debug.rawValue: .debug,
		 LogLevel.Trace.rawValue: .info,
		 LogLevel.Message.rawValue: .info,
		 LogLevel.Warning.rawValue: .error,
		 LogLevel.Error.rawValue: .error,
		 LogLevel.Fatal.rawValue: .fault]
	
	public class func debug(_ message: String) {
		instance.service.debug(message: message)
	}
	public class func info(_ message: String) {
		instance.service.message(message: message)
	}
	public class func warn(_ message: String) {
		instance.service.warning(message: message)
	}
	public class func error(_ message: String) {
		instance.service.error(message: message)
	}
	public class func fatal(_ message: String) {
		instance.service.fatal(message: message)
	}
	
	private func output(_ message: String, _ level: Int, _ domain: String = Bundle.main.bundleIdentifier!) {
        let log = "[\(domain)][\(levelToStrings[level] ?? "Unkown")] \(message)\n"
        os_log("%{public}@", type: levelToOSleLogLevel[level] ?? .info, log)
    }
    
    func onLogMessageWritten(logService: LoggingService, domain: String, level: LogLevel, message: String) {
		output(message, level.rawValue, domain)
	}

	public class func stackTrace() {
		Thread.callStackSymbols.forEach { print($0) }
	}
	
	// Debug
	public class func cdlog(_ message: String) {
		info("cdes>\(message)")
	}
	public class func bmlog(_ message: String) {
		info("bmar>\(message)")
	}
	public class func qelog(_ message: String) {
		info("qarg>\(message)")
	}
		
}
