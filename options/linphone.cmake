option(ENABLE_NLS "Enable internationalization of Linphone and Liblinphone." ON)
add_feature_info("NLS" ENABLE_NLS "Enable internationalization of Linphone and Liblinphone. (Only for the desktop target)")

option(ENABLE_RELATIVE_PREFIX "Makes liblinphone and mediastreamer look for their ressources relatively to their location. (MacOSX only)" OFF)
add_feature_info("Relative prefix" ENABLE_RELATIVE_PREFIX "Makes liblinphone and mediastreamer look for their ressources relatively to their location. (MacOSXonly)")
