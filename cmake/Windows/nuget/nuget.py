#!/usr/bin/env python

#
# Copyright (c) 2012-2019 Belledonne Communications SARL.
#
# This file is part of linphone-sdk.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#


import argparse
import glob
import os
import os.path
import shutil
import sys


class PlatformListAction(argparse.Action):

    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, values)


def handle_remove_read_only(func, path, exc):
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        func(path)
    else:
        raise


def main(argv=None):
    if argv is None:
        argv = sys.argv
    argparser = argparse.ArgumentParser(
        description="Generate nuget package of Linphone SDK.")
    argparser.add_argument(
        '-s', '--sdk_dir', default="OUTPUT", help="The path where to find the built SDK", dest='sdk_dir')
    argparser.add_argument(
        '-cs', '--cswrapper_dir', default="CsWrapper", help="The path where to find the built C# wrapper", dest='cswrapper_dir')
    argparser.add_argument(
        '-t', '--target', default="Linphone", help="The target to package (the windows runtime whose metadata will be exported)", dest='target')
    argparser.add_argument(
        '-v', '--version', default='1.0.0', help="The version of the nuget package to generate", dest='version')
    argparser.add_argument(
        '-w', '--work_dir', default="WORK/nuget", help="The path where the work will be done to generate the nuget package", dest='work_dir')

    args, additional_args2 = argparser.parse_known_args()

    target_winmd = "Belledonne.Linphone"
    target_id = "LinphoneSDK"
    target_desc = "Linphone SDK"

    # Create work dir structure
    work_dir = os.path.abspath(args.work_dir)
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir, ignore_errors=False, onerror=handle_remove_read_only)
    os.makedirs(os.path.join(work_dir, 'lib', 'uap10.0'))
    os.makedirs(os.path.join(work_dir, 'build', 'uap10.0', 'x86'))

    # Copy SDK content to nuget package structure
    sdk_dir = os.path.abspath(args.sdk_dir)
    platform_dir = 'linphone-sdk\desktop'
    grammars = glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'Belr', 'grammars', '*_grammar' ))
    sounds = glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'sounds', 'linphone', '*.wav' ))
    sounds += glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'sounds', 'linphone', '*.mkv' ))
    rings = glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'sounds', 'linphone', 'rings', '*.wav' ))
    rings += glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'sounds', 'linphone', 'rings', '*.mkv' ))
    images = glob.glob(os.path.join(sdk_dir, platform_dir, 'share', 'images', '*.jpg' ))
    dlls = glob.glob(os.path.join(sdk_dir, platform_dir, 'bin', '*.dll'))
    dlls += glob.glob(os.path.join(sdk_dir, platform_dir, 'lib', '*.dll'))
    dlls += glob.glob(os.path.join(sdk_dir, platform_dir, 'lib', 'mediastreamer', 'plugins', '*.dll'))
    pdbs = glob.glob(os.path.join(sdk_dir, platform_dir, 'bin', '*.pdb'))
    pdbs += glob.glob(os.path.join(sdk_dir, platform_dir, 'lib', '*.pdb'))
    pdbs += glob.glob(os.path.join(sdk_dir, platform_dir, 'lib', 'mediastreamer', 'plugins', '*.pdb'))
    wrappers = []
    if target_id == "LinphoneSDK":
        wrappers += glob.glob(os.path.join(args.cswrapper_dir, 'bin', 'x86', '*', '*.dll'))
        wrappers += glob.glob(os.path.join(args.cswrapper_dir, 'bin', 'x86', '*', '*.pdb'))
        wrappers += glob.glob(os.path.join(args.cswrapper_dir, 'bin', 'x86', '*', '*.XML'))
    if not os.path.exists(os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'belr', 'grammars')):
            os.makedirs(os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'belr', 'grammars'))
    if not os.path.exists(os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'rings')):
            os.makedirs(os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'rings'))

    for grammar in grammars:
        shutil.copy(grammar, os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'belr', 'grammars'))
    for sound in sounds:
        shutil.copy(sound, os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets'))
    for ring in rings:
        shutil.copy(ring, os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets', 'rings'))
    for image in images:
        shutil.copy(image, os.path.join(work_dir, 'contentFiles', 'AppX', 'Assets'))
    for dll in dlls:
        shutil.copy(dll, os.path.join(work_dir, 'build', 'uap10.0', 'x86'))
    for pdb in pdbs:
        shutil.copy(pdb, os.path.join(work_dir, 'build', 'uap10.0', 'x86'))
    for wrap in wrappers:
        shutil.copy(wrap, os.path.join(work_dir, 'lib', 'uap10.0'))

    # Write targets file
    targets = """<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!-- Note: This file will be injected into the target project -->
  <!-- Includes the native dll files as content to make sure that they are included in the app package -->
  <Target Name="CopyNativeLibraries" AfterTargets="ResolveAssemblyReferences">
    <ItemGroup>
      <Content Include="$(MSBuildThisFileDirectory)\$(Platform)\*.dll;$(MSBuildThisFileDirectory)\$(Platform)\*.pdb">
        <CopyToOutputDirectory>Always</CopyToOutputDirectory>
      </Content>
    </ItemGroup>
  </Target>  
  <ItemGroup>
    <RequiredFiles Include="$(MSBuildThisFileDirectory)\..\..\content\**\*" />
    <None Include="@(RequiredFiles)">
      <Link>%(RecursiveDir)%(FileName)%(Extension)</Link>
      <CopyToOutputDirectory>Always</CopyToOutputDirectory>
    </None>  
  </ItemGroup>
</Project>"""
    f = open(os.path.join(work_dir, 'build', 'uap10.0', target_id + '.targets'), 'w')
    f.write(targets)
    f.close()

    # Write nuspec file
    nuspec = """<?xml version="1.0"?>
<package>
  <metadata>
    <id>{target_id}</id>
    <version>{version}</version>
    <authors>Belledonne Communications</authors>
    <owners>Belledonne Communications</owners>
    <licenseUrl>https://www.gnu.org/licenses/gpl-3.0.html</licenseUrl>
    <projectUrl>https://linphone.org/</projectUrl>
    <iconUrl>https://raw.githubusercontent.com/BelledonneCommunications/linphone-windows10/master/Linphone/Assets/logo-BC.png</iconUrl>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>{target_desc}</description>
    <releaseNotes>Nothing new</releaseNotes>
    <copyright>Copyright 2017-2020 Belledonne Communications</copyright>
    <tags>SIP</tags>
    <contentFiles>
      <files include="**/*grammar" buildAction="EmbeddedResource" />
    </contentFiles>
  </metadata>
  <files>
    <file src="contentFiles\**" target="content\" />
    <file src="lib\**" target="lib\" />
    <file src="build\**" target="build\" />
  </files>
</package>""".format(version=args.version, target_id=target_id, target_desc=target_desc)
    f = open(os.path.join(work_dir, target_id + '.nuspec'), 'w')
    f.write(nuspec)
    f.close()


if __name__ == "__main__":
    sys.exit(main())
