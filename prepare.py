#!/usr/bin/env python

############################################################################
# prepare.py
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

import argparse
import copy
import imp
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from distutils.spawn import find_executable
from distutils.version import LooseVersion
from logging import error, warning, info, INFO, basicConfig
from subprocess import Popen, PIPE



class Target:

    def __init__(self, name, work_dir='WORK'):
        self.name = name
        self.output = 'OUTPUT'
        self.generator = None
        self.platform_name = None
        self.config_file = None
        self.toolchain_file = None
        self.required_build_platforms = None
        self.additional_args = []
        self.packaging_args = None
        self.work_dir = work_dir + '/' + self.name
        self.abs_work_dir = os.getcwd() + '/' + self.work_dir
        self.cmake_dir = self.work_dir + '/cmake'
        self.abs_cmake_dir = os.getcwd() + '/' + self.cmake_dir
        self.external_source_path = None
        self.lazy_install_message = True

    def output_dir(self):
        output_dir = self.output
        if not os.path.isabs(self.output):
            top_dir = os.getcwd()
            output_dir = os.path.join(top_dir, self.output)
        if platform.system() == 'Windows':
            output_dir = output_dir.replace('\\', '/')
        return output_dir

    def cmake_command(self, build_type, args, additional_args, verbose=True):
        current_path = os.path.dirname(os.path.realpath(__file__))
        if args.generator is not None:
            self.generator = args.generator # The user defined generator takes the precedence over the default target one
        cmd = ['cmake', current_path]
        if self.generator is not None:
            cmd += ['-G', self.generator]
        if self.platform_name is not None:
            cmd += ['-A', self.platform_name]
        cmd += ['-DCMAKE_BUILD_TYPE=' + build_type]
        cmd += ['-DCMAKE_PREFIX_PATH=' + self.output_dir(), '-DCMAKE_INSTALL_PREFIX=' + self.output_dir()]
        cmd += ['-DCMAKE_NO_SYSTEM_FROM_IMPORTED=YES']
        cmd += ['-DLINPHONE_BUILDER_WORK_DIR=' + self.abs_work_dir]
        if args.ccache:
            cmd += ['-DCMAKE_C_COMPILER_LAUNCHER=ccache']
            cmd += ['-DCMAKE_CXX_COMPILER_LAUNCHER=ccache']
        if self.toolchain_file is not None:
            cmd += ['-DCMAKE_TOOLCHAIN_FILE=' + self.toolchain_file]
        if self.lazy_install_message:
            cmd += ['-DCMAKE_INSTALL_MESSAGE=LAZY']
        if self.config_file is not None:
            cmd += ['-DLINPHONE_BUILDER_CONFIG_FILE=' + self.config_file]
        if self.external_source_path is not None:
            if platform.system() == 'Windows':
                self.external_source_path = self.external_source_path.replace('\\', '/')
            cmd += ['-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=' + self.external_source_path]
        if args.group:
            cmd += ['-DLINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS=YES']
        if args.debug_verbose:
            cmd += ['-DENABLE_DEBUG_LOGS=YES']
        if args.list_cmake_variables:
            cmd += ['-L']
        if 'package' in vars(args) and args.package:
            cmd += ["-DENABLE_PACKAGING=YES"]
            if self.packaging_args is not None:
                cmd += self.packaging_args
        for arg in self.additional_args:
            cmd += [arg]
        for arg in additional_args:
            cmd += [arg]
        cmd_str = ''
        for w in cmd:
            if ' ' in w:
                cmd_str += ' \"' + w + '\"'
            else:
                cmd_str += ' ' + w
        if verbose:
            print(cmd_str)
        return cmd

    def clean(self):
        if os.path.isdir(self.abs_work_dir):
            shutil.rmtree(self.abs_work_dir, ignore_errors=False, onerror=self.handle_remove_read_only)
        # special hack for vpx: we have switched from inside sources build to outside, so we must clean the folder properly
        vpx_dir = os.path.join(self.external_source_path, "externals", "libvpx")
        if os.path.isfile(os.path.join(vpx_dir, "Makefile")):
            info("Cleaning vpx source directory since we are now building it from outside directory...")
            Popen("git clean -xfd".split(" "), cwd=vpx_dir).wait()

    def veryclean(self):
        self.clean()
        if os.path.isdir(self.output_dir()):
            shutil.rmtree(self.output_dir(), ignore_errors=False, onerror=self.handle_remove_read_only)

    def handle_remove_read_only(self, func, path, exc):
        if not os.access(path, os.W_OK):
            os.chmod(path, stat.S_IWUSR)
            func(path)
        else:
            raise



class TargetListAction(argparse.Action):

    def __init__(self, option_strings, targets, dest=None, nargs=0, default=None, required=False, type=None, metavar=None, help=None):
        self.targets = targets
        super(TargetListAction, self).__init__(option_strings=option_strings, dest=dest, nargs=nargs, default=default, required=required, metavar=metavar, type=type, help=help)

    def __call__(self, parser, namespace, values, option_string=None):
        if values:
            filtered_values = []
            additional_cmake_options = []
            for value in values:
                if value.startswith('-D'):
                    additional_cmake_options += [value]
                elif value not in self.targets:
                    message = ("invalid platform: {0!r} (choose from {1})".format(value, ', '.join([repr(target) for target in self.targets])))
                    raise argparse.ArgumentError(self, message)
                else:
                    filtered_values += [value]
            if filtered_values:
                setattr(namespace, self.dest, filtered_values)
            if additional_cmake_options:
                setattr(namespace, 'additional_cmake_options', additional_cmake_options)

class ToggleAction(argparse.Action):
    def __call__(self, parser, ns, values, option):
        setattr(ns, self.dest, option[2:4] != 'no')

class Preparator:

    def __init__(self, targets={}, default_targets=[], virtual_targets={}):
        basicConfig(format="%(levelname)s: %(message)s", level=INFO)
        self.targets = targets
        self.virtual_targets = virtual_targets
        self.additional_args = []
        self.missing_python_dependencies = []
        self.missing_dependencies = {}
        self.wrong_cmake_version = False
        self.release_with_debug_info = False
        self.veryclean = False
        self.show_gpl_disclaimer = False
        self.min_cmake_version = None

        self.argparser = argparse.ArgumentParser(description="Prepare build of Linphone and its dependencies.")
        self.argparser.add_argument('-c', '--clean', help="Clean a previous build instead of preparing a build.", action='store_true')
        if platform.system() != 'Windows':
            self.argparser.add_argument('-cc', '--ccache', help="Use ccache to speed up the build process.", action='store_true')
        self.argparser.add_argument('-d', '--debug', help="Prepare a debug build, eg. add debug symbols and use no optimizations.", action='store_true')
        self.argparser.add_argument('-dv', '--debug-verbose', help="Activate ms_debug logs.", action='store_true')
        self.argparser.add_argument('-f', '--force', help="Force preparation, even if working directory already exist.", action='store_true')
        self.argparser.add_argument('-G', '--generator', help="CMake build system generator (default: let CMake choose, use cmake -h to get the complete list).", default=None, dest='generator')
        self.argparser.add_argument('-g', '--group', help="Group Linphone related builders.", action='store_true')
        self.argparser.add_argument('-L', '--list-cmake-variables', help="List non-advanced CMake cache variables.", action='store_true', dest='list_cmake_variables')
        self.argparser.add_argument('-lf', '--list-features', help="List optional features and their default values.", action='store_true', dest='list_features')
        self.argparser.add_argument('--tunnel', '--no-tunnel', help="Enable/Disable Tunnel.", action=ToggleAction, nargs=0)

        if not default_targets:
            default_targets = list(self.targets.keys())
        if len(self.targets) > 1:
            self.argparser.add_argument('target', nargs='*', action=TargetListAction, default=default_targets, targets=self.available_targets(),
                help="The target(s) to build for (default is '{0}'). Space separated targets in list: {1}.".format(' '.join(default_targets), ', '.join(self.available_targets())))

        self.argv = sys.argv[1:]
        self.load_user_config()
        self.load_project_config()

    def load_config(self, config):
        if os.path.isfile(config):
            argv = open(config).read().split()
            info("Loaded '{}' configuration: {}".format(config, argv))
            self.argv = argv + self.argv

    def load_project_config(self):
        self.load_config(os.path.join(os.getcwd(), "prepare.conf"))

    def load_user_config(self):
        self.load_config(os.path.join(os.getcwd(), "prepare.conf.user"))

    def available_targets(self):
        targets = [target for target in self.targets.keys()]
        targets += [target for target in self.virtual_targets.keys()]
        return targets

    def parse_args(self):
        self.args, self.user_additional_args = self.argparser.parse_known_args(self.argv)
        if platform.system() == 'Windows':
            self.args.ccache = False
        if hasattr(self.args, 'additional_cmake_options'):
            self.user_additional_args += self.args.additional_cmake_options
        new_targets = []
        for target_name in self.args.target:
            if target_name in self.virtual_targets.keys():
                new_targets += self.virtual_targets[target_name]
            else:
                new_targets += [target_name]
        self.args.target = list(set(new_targets))

    def check_python_module_is_present(self, modname):
        try:
            imp.find_module(modname)
            return True
        except ImportError:
            self.missing_python_dependencies += [modname]
            return False

    def check_is_installed(self, binary, prog, warn=True):
        if not find_executable(binary):
            if warn:
                self.missing_dependencies[binary] = prog
                #error("Could not find {}. Please install {}.".format(binary, prog))
            return False
        return True

    def check_cmake_version(self):
        cmake = find_executable('cmake')
        p = Popen([cmake, '--version'], shell=False, stdout=PIPE, universal_newlines=True)
        p.wait()
        if p.returncode != 0:
            self.wrong_cmake_version = True
        else:
            cmake_version = p.stdout.readlines()[0].split()[-1]
            if LooseVersion(cmake_version) < LooseVersion(self.min_cmake_version):
                self.wrong_cmake_version = True
        return not self.wrong_cmake_version

    def check_environment(self, submodule_directory_to_check=None):
        ret = 0

        # at least FFmpeg requires no whitespace in sources path...
        if " " in os.path.dirname(os.path.realpath(__file__)):
            error("Invalid location: path should not contain any spaces.")
            ret = 1

        ret |= not self.check_is_installed('cmake', 'cmake')
        if not ret and self.min_cmake_version is not None:
            ret |= not self.check_cmake_version()

        if submodule_directory_to_check is None:
            submodule_directory_to_check = "submodules/linphone/mediastreamer2/src"
        if not os.path.isdir(submodule_directory_to_check):
            error("Missing some git submodules. Did you run:\n\tgit submodule update --init --recursive")
            ret = 1

        return ret

    def show_environment_errors(self):
        self.show_wrong_cmake_version()
        self.show_missing_dependencies()
        self.show_missing_python_dependencies()

    def show_wrong_cmake_version(self):
        if self.wrong_cmake_version:
            error("You need at leat CMake version {}.".format(self.min_cmake_version))

    def show_missing_dependencies(self):
        if self.missing_dependencies:
            error("The following binaries are missing: {}. Please install them.".format(' '.join(self.missing_dependencies.keys())))

    def show_missing_python_dependencies(self):
        if self.missing_python_dependencies:
            error("The following python modules are missing: {}. Please install them using:\n\tpip install {}".format(
                ' '.join(self.missing_python_dependencies), ' '.join(self.missing_python_dependencies)))

    def gpl_disclaimer(self):
        if not self.show_gpl_disclaimer:
            return
        cmakecache = os.path.join(self.first_target().abs_work_dir, 'cmake', 'CMakeCache.txt')
        gpl_third_parties_enabled = "ENABLE_GPL_THIRD_PARTIES:BOOL=YES" in open(
           cmakecache).read() or "ENABLE_GPL_THIRD_PARTIES:BOOL=ON" in open(cmakecache).read()

        if gpl_third_parties_enabled:
            warning("\n***************************************************************************"
                    "\n***************************************************************************"
                    "\n***** CAUTION, this liblinphone SDK is built using 3rd party GPL code *****"
                    "\n***** Even if you acquired a proprietary license from Belledonne      *****"
                    "\n***** Communications, this SDK is GPL and GPL only.                   *****"
                    "\n***** To disable 3rd party gpl code, please use:                      *****"
                    "\n***** $ ./prepare.py -DENABLE_GPL_THIRD_PARTIES=NO                    *****"
                    "\n***************************************************************************"
                    "\n***************************************************************************")
        else:
            warning("\n***************************************************************************"
                    "\n***************************************************************************"
                    "\n***** Linphone SDK without 3rd party GPL software                     *****"
                    "\n***** If you acquired a proprietary license from Belledonne           *****"
                    "\n***** Communications, this SDK can be used to create                  *****"
                    "\n***** a proprietary linphone-based application.                       *****"
                    "\n***************************************************************************"
                    "\n***************************************************************************")

    def list_feature_target(self):
        return None

    def first_target(self):
        return self.targets[self.args.target[0]]

    def generator(self):
        return self.first_target().generator

    def get_additional_args(self):
        # Append user_additional_args to additional_args so that the user's option take the priority
        return self.additional_args + self.user_additional_args

    def list_features_with_args(self, args, additional_args):
        tmpdir = tempfile.mkdtemp(prefix="linphone-prepare")
        tmptarget = self.list_feature_target()
        if tmptarget is None:
            tmptarget = self.first_target()
        tmptarget.abs_cmake_dir = tmpdir
        option_regex = re.compile("ENABLE_(.*):(.*)=(.*)")
        options = {}
        ended = True
        if args.debug:
            build_type = 'Debug'
        elif self.release_with_debug_info:
            build_type = 'RelWithDebInfo'
        else:
            build_type = 'Release'

        p = Popen(tmptarget.cmake_command(build_type, args, additional_args, verbose=False), cwd=tmpdir, shell=False, stdout=PIPE, universal_newlines=True)
        p.wait()
        if p.returncode != 0:
            sys.exit(-1)
        for line in p.stdout.readlines():
            match = option_regex.match(line)
            if match is not None:
                (name, typeof, value) = match.groups()
                options["ENABLE_{}".format(name)] = value
                ended &= (value == 'ON')
        shutil.rmtree(tmpdir)
        return (options, ended)

    def list_features(self, args, additional_args):
        args.list_cmake_variables = True
        additional_args_copy = additional_args
        options = {}
        info("Searching for available features...")
        # We have to iterate multiple times to activate ALL options, so that options depending
        # of others are also listed (cmake_dependent_option macro will not output options if
        # prerequisite is not met)
        while True:
            (options, ended) = self.list_features_with_args(args, additional_args_copy)
            if ended or (len(options) == len(additional_args_copy)):
                break
            else:
                additional_args_copy = []
                # Activate ALL available options
                for k in options.keys():
                    additional_args_copy.append("-D{}=ON".format(k))

        # Now that we got the list of ALL available options, we must correct default values
        # Step 1: all options are turned off by default
        for x in options.keys():
            options[x] = 'OFF'
        # Step 2: except options enabled when running with default args
        (options_tmp, ended) = self.list_features_with_args(args, additional_args)
        final_dict = dict(options, **options_tmp)

        notice_features = "Here are available features:"
        for k, v in final_dict.items():
            notice_features += "\n\t{}={}".format(k, v)
        info(notice_features)
        info("To enable some feature, please use -DENABLE_SOMEOPTION=ON (example: -DENABLE_OPUS=ON)")
        info("Similarly, to disable some feature, please use -DENABLE_SOMEOPTION=OFF (example: -DENABLE_OPUS=OFF)")

    def target_clean(self, target):
        if self.veryclean:
            target.veryclean()
        else:
            target.clean()

    def target_prepare(self, target):
        if target.required_build_platforms is not None:
            if not platform.system() in target.required_build_platforms:
                print("Cannot build target '{target}' on '{bad_build_platform}' build platform. Build it on one of {good_build_platforms}.".format(
                    target=target.name, bad_build_platform=platform.system(), good_build_platforms=', '.join(target.required_build_platforms)))
                return 52

        if type(self.args.debug) is str:
            build_type = self.args.debug
        elif self.args.debug:
            build_type = 'Debug'
        elif self.release_with_debug_info:
            build_type = 'RelWithDebInfo'
        else:
            build_type = 'Release'

        if not os.path.isdir(target.abs_cmake_dir):
            os.makedirs(target.abs_cmake_dir)

        self.prepare_tunnel()

        p = Popen(target.cmake_command(build_type, self.args, self.get_additional_args()), cwd=target.abs_cmake_dir, shell=False)
        p.communicate()

        if target.generator is None:
            # No generator has been specified, find the one CMake has used
            cmakecache = os.path.join(target.abs_work_dir, 'cmake', 'CMakeCache.txt')
            generator_regex = re.compile("CMAKE_GENERATOR:(.*)=(.*)", flags=re.MULTILINE)
            content = open(cmakecache).read()
            match = generator_regex.search(content)
            if match:
                target.generator = match.group(2)

        return p.returncode

    def clean(self):
        for target_name, target in self.targets.items():
            self.target_clean(target)
        return 0

    def prepare(self):
        ret = 0

        if self.args.list_features:
            self.list_features(self.args, self.get_additional_args())
            sys.exit(0)

        if self.args.force is False:
            for target_name, target in self.targets.items():
                if os.path.isdir(target.abs_cmake_dir):
                    print("Working directory {} already exists. Please remove it (option -c) before re-executing prepare.py "
                          "to avoid conflicts between executions, or force execution (option -f) if you are aware of consequences.".format(target.cmake_dir))
                    ret = 51
                    break

        if ret == 0:
            for target_name in self.args.target:
                ret = self.target_prepare(self.targets[target_name])
                if ret != 0:
                    break
        if ret != 0:
            if ret == 51:
                if os.path.isfile('Makefile'):
                    Popen("make help-prepare-options".split(" ")).wait()
                ret = 0
            return ret
        # Only generated makefile if we are using Ninja or Makefile
        if self.generator().endswith('Ninja'):
            if not self.check_is_installed("ninja", "ninja"):
                return 1
            self.generate_makefile('ninja -C')
            info("You can now run 'make' to build.")
        elif self.generator().endswith("Unix Makefiles"):
            self.generate_makefile('$(MAKE) -C')
            info("You can now run 'make' to build.")
        elif self.generator().endswith("Xcode"):
            self.generate_makefile('xcodebuild -project', 'Project.xcodeproj')
            info("You can now run 'make' to build.")
        elif self.generator().startswith("Visual Studio"):
            self.generate_vs_solution()
        else:
            warning("Not generating meta-makefile for generator {}.".format(self.generator()))
        self.gpl_disclaimer()
        return ret

    def run(self):
        if self.args.clean:
            return self.clean()
        else:
            return self.prepare()

    def generate_makefile(self, generator, project_file=''):
        pass

    def generate_vs_solution(self):
        pass

    def prepare_tunnel(self):
        if self.args.tunnel:
            if not os.path.isdir("submodules/tunnel"):
                info("Tunnel wanted but not found yet, trying to clone it...")
                p = Popen("git clone gitosis@git.linphone.org:tunnel.git submodules/tunnel".split(" "))
                p.wait()
                if p.returncode != 0:
                    error("Could not clone tunnel. Please see http://www.belledonne-communications.com/voiptunnel.html")
                    return 1
            info("Tunnel enabled.")
            self.additional_args += ["-DENABLE_TUNNEL=YES"]
