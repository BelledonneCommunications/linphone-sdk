// Signing APK Release
RELEASE_STORE_FILE=""
RELEASE_STORE_PASSWORD=
RELEASE_KEY_ALIAS=
RELEASE_KEY_PASSWORD=
#source:https://docs.gradle.org/current/userguide/build_environment.html#sec:configuring_jvm_memory
org.gradle.jvmargs=-XX:+HeapDumpOnOutOfMemoryError -Dfile.encoding=UTF-8
android.useAndroidX=true
# Automatically convert third-party libraries to use AndroidX
android.enableJetifier=true
android.defaults.buildfeatures.buildconfig=true
android.nonTransitiveRClass=false
android.nonFinalResIds=false