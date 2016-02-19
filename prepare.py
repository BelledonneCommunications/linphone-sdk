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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

import argparse
import os
import platform
import shutil
import stat
import subprocess
import sys


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
        self.work_dir = work_dir + '/' + self.name
        self.abs_work_dir = os.getcwd() + '/' + self.work_dir
        self.cmake_dir = self.work_dir + '/cmake'
        self.abs_cmake_dir = os.getcwd() + '/' + self.cmake_dir

    def output_dir(self):
        output_dir = self.output
        if not os.path.isabs(self.output):
            top_dir = os.getcwd()
            output_dir = os.path.join(top_dir, self.output)
        if platform.system() == 'Windows':
            output_dir = output_dir.replace('\\', '/')
        return output_dir

    def cmake_command(self, build_type, latest, list_cmake_variables, additional_args, verbose=True):
        current_path = os.path.dirname(os.path.realpath(__file__))
        cmd = ['cmake', current_path]
        if self.generator is not None:
            cmd += ['-G', self.generator]
        if self.platform_name is not None:
            cmd += ['-A', self.platform_name]
        cmd += ['-DCMAKE_BUILD_TYPE=' + build_type]
        cmd += ['-DCMAKE_PREFIX_PATH=' + self.output_dir(), '-DCMAKE_INSTALL_PREFIX=' + self.output_dir()]
        cmd += ['-DLINPHONE_BUILDER_WORK_DIR=' + self.abs_work_dir]
        if self.toolchain_file is not None:
            cmd += ['-DCMAKE_TOOLCHAIN_FILE=' + self.toolchain_file]
        if self.config_file is not None:
            cmd += ['-DLINPHONE_BUILDER_CONFIG_FILE=' + self.config_file]
        if latest:
            cmd += ['-DLINPHONE_BUILDER_LATEST=YES']
        if list_cmake_variables:
            cmd += ['-L']
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

    def build_instructions(self, debug=False):
        if self.generator is not None and self.generator.startswith('Visual Studio'):
            config = "Release"
            if debug:
                config = "Debug"
            return "Open the \"{cmake_dir}/Project.sln\" Visual Studio solution and build with the \"{config}\" configuration".format(cmake_dir=self.cmake_dir, config=config)
        else:
            if self.generator in [None, "Unix Makefiles"]:
                builder = "make"
            elif self.generator == "Ninja":
                builder = "ninja"
            else:
                return "Unknown generator. Files have been generated in {cmake_dir}".format(cmake_dir=self.cmake_dir)
            return "Run the following command to build:\n\t{builder} -C {cmake_dir}".format(builder=builder, cmake_dir=self.cmake_dir)


class DesktopTarget(Target):

    def __init__(self):
        Target.__init__(self, 'desktop')
        self.config_file = 'configs/config-desktop.cmake'
        if platform.system() == 'Windows':
            self.generator = 'Visual Studio 12 2013'


class FlexisipTarget(Target):

    def __init__(self):
        Target.__init__(self, 'flexisip')
        self.required_build_platforms = ['Linux', 'Darwin']
        self.config_file = 'configs/config-flexisip.cmake'
        self.additional_args = ['-DLINPHONE_BUILDER_TARGET=flexisip']


class FlexisipRpmTarget(Target):

    def __init__(self):
        Target.__init__(self, 'flexisip-rpm')
        self.required_build_platforms = ['Linux', 'Darwin']
        self.config_file = 'configs/config-flexisip-rpm.cmake'
        self.additional_args = ['-DLINPHONE_BUILDER_TARGET=flexisip']


class PythonTarget(Target):

    def __init__(self):
        Target.__init__(self, 'python')
        self.config_file = 'configs/config-python.cmake'
        if platform.system() == 'Windows':
            self.generator = 'Visual Studio 9 2008'


class PythonRaspberryTarget(Target):

    def __init__(self):
        Target.__init__(self, 'python-raspberry')
        self.required_build_platforms = ['Linux']
        self.config_file = 'configs/config-python-raspberry.cmake'
        self.toolchain_file = 'toolchains/toolchain-raspberry.cmake'


targets = {}
targets['desktop'] = DesktopTarget()
targets['flexisip'] = FlexisipTarget()
targets['flexisip-rpm'] = FlexisipRpmTarget()
targets['python'] = PythonTarget()
targets['python-raspberry'] = PythonRaspberryTarget()
target_names = sorted(targets.keys())


def run(target, debug, latest, list_cmake_variables, force_build, additional_args):
    build_type = 'Debug' if debug else 'Release'

    if target.required_build_platforms is not None:
        if not platform.system() in target.required_build_platforms:
            print("Cannot build target '{target}' on '{bad_build_platform}' build platform. Build it on one of {good_build_platforms}.".format(
                target=target.name, bad_build_platform=platform.system(), good_build_platforms=', '.join(target.required_build_platforms)))
            return 52

    if os.path.isdir(target.abs_cmake_dir):
        if force_build is False:
            print("Working directory {} already exists. Please remove it (option -C or -c) before re-executing CMake "
                  "to avoid conflicts between executions, or force execution (option -f) if you are aware of consequences.".format(target.cmake_dir))
            return 51
    else:
        os.makedirs(target.abs_cmake_dir)

    proc = subprocess.Popen(target.cmake_command(
        build_type, latest, list_cmake_variables, additional_args), cwd=target.abs_cmake_dir, shell=False)
    proc.communicate()
    return proc.returncode


def main(argv=None):
    if argv is None:
        argv = sys.argv
    argparser = argparse.ArgumentParser(description="Prepare build of Linphone and its dependencies.")
    argparser.add_argument(
        '-c', '--clean', help="Clean a previous build instead of preparing a build.", action='store_true')
    argparser.add_argument(
        '-C', '--veryclean', help="Clean a previous build and its installation directory.", action='store_true')
    argparser.add_argument('-d', '--debug', help="Prepare a debug build.", action='store_true')
    argparser.add_argument(
        '-f', '--force', help="Force preparation, even if working directory already exist.", action='store_true')
    argparser.add_argument('-l', '--latest', help="Build latest versions of all dependencies.", action='store_true')
    argparser.add_argument('-o', '--output', help="Specify output directory.")
    argparser.add_argument('-G', '--generator', metavar='generator', help="CMake generator to use.")
    argparser.add_argument('-L', '--list-cmake-variables',
                           help="List non-advanced CMake cache variables.", action='store_true', dest='list_cmake_variables')
    argparser.add_argument('target', choices=target_names, help="The target to build.")
    args, additional_args = argparser.parse_known_args()

    target = targets[args.target]
    if args.generator:
        target.generator = args.generator
    if args.output:
        target.output = args.output

    retcode = 0
    if args.veryclean:
        target.veryclean()
    elif args.clean:
        target.clean()
    else:
        retcode = run(target, args.debug, args.latest, args.list_cmake_variables, args.force, additional_args)
        if retcode == 0:
            print("\n" + target.build_instructions(args.debug))
    return retcode

if __name__ == "__main__":
    sys.exit(main())
