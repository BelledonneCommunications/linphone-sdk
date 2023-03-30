[binaries]
c = ['clang']
cpp = ['clang++']
ar = ['ar']
strip = ['strip']
pkg-config = ['pkg-config']

[host_machine]
system = '@EP_SYSTEM_NAME@'
cpu_family = '@EP_SYSTEM_PROCESSOR@'
cpu = '@EP_SYSTEM_PROCESSOR@'
endian = '@EP_SYSTEM_ENDIAN@'

[built-in options]
c_args = ['-arch', '@CMAKE_OSX_ARCHITECTURES@', '-isysroot', '@CMAKE_OSX_SYSROOT@', '@EP_OSX_DEPLOYMENT_TARGET@' @EP_ADDITIONAL_FLAGS@]
c_link_args = ['-arch', '@CMAKE_OSX_ARCHITECTURES@', '-isysroot', '@CMAKE_OSX_SYSROOT@', '@EP_OSX_DEPLOYMENT_TARGET@' @EP_ADDITIONAL_FLAGS@]
