option(ENABLE_NLS "Enable internationalization of Linphone and Liblinphone." ON)
add_feature_info("NLS" ENABLE_NLS "Enable internationalization of Linphone and Liblinphone. (Only for the desktop target)")

if (APPLE AND NOT IOS)
	option(ENABLE_RELATIVE_PREFIX "liblinphone and mediastreamer will look for their respective ressources relatively to their location." OFF)
	add_feature_info("Relative prefix" ENABLE_RELATIVE_PREFIX "liblinphone and mediastreamer will look for their respective ressources relatively to their location.")
endif()
