
set(hiredis_filename "v0.11.0.tar.gz")
set(EP_hiredis_URL "https://github.com/redis/hiredis/archive/${hiredis_filename}")
set(EP_hiredis_URL_HASH "SHA1=694b6d7a6e4ea7fb20902619e9a2423c014b37c1")
set(EP_hiredis_BUILD_METHOD "rpm")

set(EP_hiredis_SPEC_FILE "hiredis.spec" )
set(EP_hiredis_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/${EP_hiredis_SPEC_FILE}" )

#create source dir and copy the tar.gz inside
set(EP_hiredis_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_hiredis_PATCH_COMMAND ${EP_hiredis_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${LINPHONE_BUILDER_WORK_DIR}/Download/EP_hiredis/${hiredis_filename}" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_hiredis_PATCH_COMMAND ${EP_hiredis_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_hiredis_CONFIG_H_FILE} "<BINARY_DIR>")
