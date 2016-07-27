#!/usr/bin/python

# Copyright (C) 2014 Belledonne Communications SARL
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

import os
import re
import string
import sys
from setuptools import setup, Extension

name = "@PACKAGE_NAME@"
version = "@BUILD_VERSION@"
data_files = "@LINPHONE_DATA_FILES@".split(';')

setup(name = name,
	version = version,
	description = 'Linphone package for Python',
	author = "Belledonne Communications",
	author_email = "contact@belledonne-communications.com",
	url = "http://www.linphone.org/",
	packages = ['linphone'],
	ext_package = 'linphone',
	package_data = {'linphone': data_files},
	zip_safe = True,
	keywords = ["sip", "voip"],
	classifiers = [
		"Development Status :: 5 - Production/Stable",
		"Environment :: Other Environment",
		"Intended Audience :: Developers",
		"Intended Audience :: Telecommunications Industry",
		"License :: OSI Approved :: GNU General Public License v2 or later (GPLv2+)",
		"Natural Language :: English",
		"Operating System :: Microsoft :: Windows",
		"Programming Language :: C",
		"Programming Language :: C++",
		"Programming Language :: Python :: 2.7",
		"Topic :: Communications :: Chat",
		"Topic :: Communications :: Internet Phone",
		"Topic :: Communications :: Telephony"
	]
)
