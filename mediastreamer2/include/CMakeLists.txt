file(GLOB HEADER_FILES "mediastreamer2/*.h")

install(FILES ${HEADER_FILES}
        DESTINATION include/mediastreamer2
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
