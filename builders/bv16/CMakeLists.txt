cmake_minimum_required(VERSION 3.0)

project(BV16
  VERSION 2.1
  LANGUAGES C
)
set(FBV16_MAJOR_VERSION 2)
set(FBV16_MINOR_VERSION 1)
set(FBV16_VERSION
  ${FBV16_MAJOR_VERSION}.${FBV16_MINOR_VERSION})
 
include_directories(${CMAKE_BINARY_DIR} ./bv16)
include_directories(${CMAKE_BINARY_DIR} ./bvcommon)

option(ENABLE_STATIC "Build static library (default is shared library)." OFF)

# add_subdirectory(bv16)
# add_subdirectory(bvcommon)

set( BV16_SOURCE_FILES
    ./bv16/bitpack.c
    ./bv16/bv.c
    ./bv16/coarptch.c
    ./bv16/decoder.c
    ./bv16/encoder.c
    ./bv16/excdec.c
    ./bv16/excquan.c
    ./bv16/fineptch.c
    ./bv16/g192.c
    ./bv16/gaindec.c
    ./bv16/gainquan.c
    ./bv16/levelest.c
    ./bv16/lspdec.c
    ./bv16/lspquan.c
    ./bv16/plc.c
    ./bv16/postfilt.c
    ./bv16/ptquan.c
    ./bv16/tables.c
    ./bvcommon/a2lsp.c
    ./bvcommon/allpole.c
    ./bvcommon/allzero.c
    ./bvcommon/autocor.c
    ./bvcommon/cmtables.c
    ./bvcommon/levdur.c
    ./bvcommon/lsp2a.c
    ./bvcommon/ptdec.c
    ./bvcommon/stblchck.c
    ./bvcommon/stblzlsp.c
    ./bvcommon/utility.c
)

add_compile_options(-O -Wall)
add_compile_options(-DWMOPS=0) 
add_compile_options(-DG192BITSTREAM=0)


if(MSVC)
  list(APPEND BV16_SOURCE_FILES bv16.def)
endif(MSVC)

if(ENABLE_STATIC)
  add_library(bv16 STATIC ${BV16_SOURCE_FILES})
  add_executable(BroadVoice16  ${BV16_SOURCE_FILES})
else()
  add_library(bv16 SHARED ${BV16_SOURCE_FILES})
  add_executable(BroadVoice16  ${BV16_SOURCE_FILES})
else()
  if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bv16.pdb
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
      )
    endif()
  endif()
endif()

if(APPLE)
  target_link_libraries(bv16 "m")
endif()

# if(APPLE)
#   target_link_libraries(bv16 "-framework Cocoa -lm")
# endif()
install(TARGETS bv16
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(TARGETS BroadVoice16
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


file(GLOB BV16_HEADER_FILES "bv16/*.h")
file(GLOB BV16_COMMON_HEADER_FILES "bvcommon/*.h")



install(FILES ${BV16_HEADER_FILES}
  DESTINATION include/bv16-floatingpoint/bv16
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(FILES ${BV16_COMMON_HEADER_FILES}
  DESTINATION include/bv16-floatingpoint/bvcommon
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
# install(FILES
#   DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/bv16
#)


# # Offer the user the choice of overriding the installation directories
# set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
# set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
# set(INSTALL_INCLUDE_DIR include CACHE PATH
#   "Installation directory for header files")
# if(WIN32 AND NOT CYGWIN)
#   set(DEF_INSTALL_CMAKE_DIR CMake)
# else()
#   set(DEF_INSTALL_CMAKE_DIR lib/CMake/FooBar)
# endif()
# set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH
#   "Installation directory for CMake files")
 
# # Make relative paths absolute (needed later on)
# foreach(p LIB BIN INCLUDE CMAKE)
#   set(var INSTALL_${p}_DIR)
#   if(NOT IS_ABSOLUTE "${${var}}")
#     set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
#   endif()
# endforeach()
 
# # set up include-directories
# include_directories(
#   "${PROJECT_SOURCE_DIR}"   # to find foo/foo.h
#   "${PROJECT_BINARY_DIR}")  # to find foo/config.h
 
# # Add sub-directories
# add_subdirectory(foo)
# add_subdirectory(bar)
 
# # The interesting stuff goes here
# # ===============================
 
# # Add all targets to the build-tree export set
# export(TARGETS foo bar
#   FILE "${PROJECT_BINARY_DIR}/FooBarTargets.cmake")
 
# # Export the package for use from the build-tree
# # (this registers the build-tree with a global CMake-registry)
# export(PACKAGE FooBar)
 
# # Create the FooBarConfig.cmake and FooBarConfigVersion files
# file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
#    "${INSTALL_INCLUDE_DIR}")
# # ... for the build tree
# set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
# configure_file(FooBarConfig.cmake.in
#   "${PROJECT_BINARY_DIR}/FooBarConfig.cmake" @ONLY)
# # ... for the install tree
# set(CONF_INCLUDE_DIRS "\${FBV16_CMAKE_DIR}/${REL_INCLUDE_DIR}")
# configure_file(FooBarConfig.cmake.in
#   "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FooBarConfig.cmake" @ONLY)
# # ... for both
# configure_file(FooBarConfigVersion.cmake.in
#   "${PROJECT_BINARY_DIR}/FooBarConfigVersion.cmake" @ONLY)
 
# # Install the FooBarConfig.cmake and FooBarConfigVersion.cmake
# install(FILES
#   "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FooBarConfig.cmake"
#   "${PROJECT_BINARY_DIR}/FooBarConfigVersion.cmake"
#   DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)
 
# # Install the export set for use with the install-tree
# install(EXPORT FooBarTargets DESTINATION
#   "${INSTALL_CMAKE_DIR}" COMPONENT dev)