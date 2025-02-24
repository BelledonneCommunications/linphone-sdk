############################################################################
# GenerateSwiftPackage.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

#########################################################################
# Generate Swift Package linphonesw
# Requires XCFrameworks (not ENABLE_FAT_BINARY) and LinphoneWrapper.swift
##########################################################################


if(NOT ENABLE_FAT_BINARY AND EXISTS "${LINPHONE_WRAPPER_SWIFT}")
	set(SWIFT_TOOLS_VERSION 5.9)
	
	set(SWIFT_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/linphone-sdk-swift-${SWIFT_PACKAGE_TARGET}")
	message(STATUS "Generating linphonesw Swift Package in ${SWIFT_PACKAGE_DIR}")
	
	file(GLOB XCFRAMEWORKS "${LINPHONESDK_NAME}/${SWIFT_PACKAGE_SOURCE_XC_FRAMEWORKS_LOCATION}/XCFrameworks/*.xcframework")
	
	set(BINARY_TARGETS "")
	set(XCFRAMEWORKS_NAMES "")

	file(MAKE_DIRECTORY "${SWIFT_PACKAGE_DIR}/XCFrameworks/")

	foreach(XCFRAMEWORK ${XCFRAMEWORKS})
		get_filename_component(TARGET_NAME ${XCFRAMEWORK} NAME_WE)
		get_filename_component(XCFRAMEWORK_NAME ${XCFRAMEWORK} NAME)
		execute_process(
			COMMAND "zip" "-r" "-y" "${SWIFT_PACKAGE_DIR}/XCFrameworks/${XCFRAMEWORK_NAME}.zip" "${XCFRAMEWORK_NAME}"
			WORKING_DIRECTORY "${LINPHONESDK_NAME}/${SWIFT_PACKAGE_SOURCE_XC_FRAMEWORKS_LOCATION}/XCFrameworks"
		)
		list(APPEND XCFRAMEWORKS_NAMES "\"${TARGET_NAME}\"")
		if (UPLOAD_SWIFT_PACKAGE)
			execute_process(
				COMMAND swift package compute-checksum "${SWIFT_PACKAGE_DIR}/XCFrameworks/${XCFRAMEWORK_NAME}.zip"
				WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
				OUTPUT_VARIABLE FRAMEWORK_ZIP_CHECKSUM
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			string(APPEND BINARY_TARGETS "
			.binaryTarget(
				name: \"${TARGET_NAME}\",
				url: \"${SWIFT_PACKAGE_BASE_URL}/linphone-sdk-swift-${SWIFT_PACKAGE_TARGET}-${LINPHONESDK_VERSION}/XCFrameworks/${XCFRAMEWORK_NAME}.zip\",
				checksum: \"${FRAMEWORK_ZIP_CHECKSUM}\"
			),
			")
		else()
			string(APPEND BINARY_TARGETS "
			.binaryTarget(
				name: \"${TARGET_NAME}\",
				path: \"XCFrameworks/${XCFRAMEWORK_NAME}.zip\"
			),
			")
		endif()
	endforeach()

	string(REPLACE ";" ", " XCFRAMEWORKS_NAMES_CSV "${XCFRAMEWORKS_NAMES}")

	configure_file(
		"${LINPHONESDK_DIR}/cmake/SwiftPackageManager/LinphoneSwiftPackage.swift.in"
		"${SWIFT_PACKAGE_DIR}/Package.swift"
		@ONLY
	)
	
	# Swift Package Manager requires at least one soure file per target.
	file(MAKE_DIRECTORY "${SWIFT_PACKAGE_DIR}/Sources/linphonexcframeworks")
	file(TOUCH "${SWIFT_PACKAGE_DIR}/Sources/linphonexcframeworks/Dummy.swift")
	
	# Copy LinphoneWrapper.swift (Swift Package Manager does not allow relative path)
	file(MAKE_DIRECTORY ${SWIFT_PACKAGE_DIR}/Sources/linphonesw)
	file(COPY "${LINPHONE_WRAPPER_SWIFT}" DESTINATION "${SWIFT_PACKAGE_DIR}/Sources/linphonesw")
	
	# Expose SDK version and branch as static Core class through extension as not exposed through local build.
	configure_file(
		"${LINPHONESDK_DIR}/cmake/SwiftPackageManager/LinphoneSdkInfos.swift.in"
		"${SWIFT_PACKAGE_DIR}/Sources/linphonesw/LinphoneSdkInfos.swift"
		@ONLY
	)
	
	# README and License file
	configure_file(
		"${LINPHONESDK_DIR}/cmake/SwiftPackageManager/README.md.in"
		"${SWIFT_PACKAGE_DIR}/README.md"
		@ONLY
	)
	file(COPY "${LINPHONESDK_DIR}/LICENSE.txt" DESTINATION "${SWIFT_PACKAGE_DIR}")
	file(WRITE ${SWIFT_PACKAGE_DIR}/VERSION "${LINPHONESDK_VERSION}")
	message(STATUS "Generated linphonesw Swift Package in ${SWIFT_PACKAGE_DIR}")


elseif(ENABLE_FAT_BINARY)
		message(STATUS "Swift Package not generated as ENABLE_FAT_BINARY is ON (XCFramework required for SPM)")
else()
		message(STATUS "Swift Package not generated as LinphoneWrapper.swift was not generated")
endif()
