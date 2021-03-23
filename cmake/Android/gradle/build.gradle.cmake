// Project information
buildDir = 'linphone-sdk/bin'

buildscript {
    repositories {
        google()
        jcenter()
        mavenCentral()
        maven {
            url "https://plugins.gradle.org/m2/"
        }
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:4.0.1'
    }
}

allprojects {
    repositories {
        google()
        jcenter()
    }
}

configurations {
    javadocDeps
}

apply plugin: 'com.android.library'

dependencies {
    implementation 'org.apache.commons:commons-compress:1.16.1'
    javadocDeps 'org.apache.commons:commons-compress:1.16.1'
    compileOnly "androidx.media:media:1.2.0"
    implementation 'androidx.annotation:annotation:1.1.0'
    compileOnly 'com.google.firebase:firebase-messaging:19.0.1'
}

static def isGeneratedJavaWrapperAvailable() {
    File coreWrapper = new File('@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@/share/linphonej/java/org/linphone/core/Core.java')
    return coreWrapper.exists()
}

def rootSdk = '@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@'
def srcDir = ['@LINPHONESDK_DIR@/mediastreamer2/java/src']
def pluginsDir = rootSdk + '/lib/mediastreamer/plugins/'
srcDir += [rootSdk + '/share/linphonej/java/']
srcDir += ['@LINPHONESDK_DIR@/liblinphone/wrappers/java/classes/']

def excludePackage = []
excludePackage.add('**/gdb.*')
excludePackage.add('**/libopenh264**')
excludePackage.add('**/LICENSE.txt')

def javaExcludes = []
javaExcludes.add('**/mediastream/MediastreamerActivity.java')

if (!isGeneratedJavaWrapperAvailable()) {
    // We have to remove some classes that requires the new java wrapper
    println("Old java wrapper detected, adding it to sources and removing some incompatible classes")

    // This classes uses the new DialPlan wrapped object
    javaExcludes.add('**/Utils.java')
    javaExcludes.add('**/H264Helper.java')

    // This classes use some of the new Java objects like Core, Call, ProxyConfig, etc...
    javaExcludes.add('**/ActivityMonitor.java')
    javaExcludes.add('**/AudioHelper.java')
    javaExcludes.add('**/BluetoothHelper.java')
    javaExcludes.add('**/BluetoothReceiver.java')
    javaExcludes.add('**/ShutdownReceiver.java')
    javaExcludes.add('**/CoreManager.java')
    javaExcludes.add('**/CoreService.java')
    javaExcludes.add('**/FirebaseMessaging.java')
    javaExcludes.add('**/FirebasePushHelper.java')
    javaExcludes.add('**/HeadsetReceiver.java')
    javaExcludes.add('**/PushNotificationUtils.java')

    // Add the previous wrapper to sources
    srcDir += ['@LINPHONESDK_DIR@/liblinphone/java/common/']
    srcDir += ['@LINPHONESDK_DIR@/liblinphone/java/impl/']
    srcDir += ['@LINPHONESDK_DIR@/liblinphone/java/j2se/']
}

def pluginsList = ""

task listPlugins() {
    fileTree(pluginsDir).visit { FileVisitDetails details -> 
        println("Found plugin: " + details.file.name)
        pluginsList = pluginsList + "\"" + details.file.name  + "\","
    }
}

project.tasks['preBuild'].dependsOn 'listPlugins'

android {
    compileSdkVersion 30
    buildToolsVersion "30.0.2"
    
    defaultConfig {
        minSdkVersion 23
        targetSdkVersion 30
        versionCode 4500
        versionName "@LINPHONESDK_VERSION@"
        setProperty("archivesBaseName", "linphone-sdk-android")
        consumerProguardFiles "${buildDir}/proguard.txt"
    }

    signingConfigs {
        release {
            storeFile file(RELEASE_STORE_FILE)
            storePassword RELEASE_STORE_PASSWORD
            keyAlias RELEASE_KEY_ALIAS
            keyPassword RELEASE_KEY_PASSWORD
        }
    }

    buildTypes {
        debug {
            minifyEnabled false
            debuggable true
            jniDebuggable true
            resValue "string", "linphone_sdk_version", "@LINPHONESDK_VERSION@-debug"
            resValue "string", "linphone_sdk_branch", "@LINPHONESDK_BRANCH@"
            buildConfigField "String[]", "PLUGINS_ARRAY", "{" + pluginsList +  "}"
        }
        release {
            signingConfig signingConfigs.release
            minifyEnabled false
            resValue "string", "linphone_sdk_version", "@LINPHONESDK_VERSION@"
            resValue "string", "linphone_sdk_branch", "@LINPHONESDK_BRANCH@"
            buildConfigField "String[]", "PLUGINS_ARRAY", "{" + pluginsList +  "}"
        }
    }

    lintOptions {
        checkReleaseBuilds false
        // Or, if you prefer, you can continue to check for errors in release builds,
        // but continue the build even when errors are found:
        abortOnError false
    }

    sourceSets {
        main {
            manifest.srcFile 'LinphoneSdkManifest.xml'
            java.srcDirs = srcDir
            aidl.srcDirs = srcDir
            assets.srcDirs = ["${buildDir}/sdk-assets/assets/"]
            renderscript.srcDirs = srcDir
            java.excludes = javaExcludes

            // Exclude some useless files and don't strip libraries, stripping will be taken care of by CopyLibs.cmake
            packagingOptions {
                excludes = excludePackage
                doNotStrip '**/*.so'
            }
        }
        debug {
            root = 'build-types/debug'
            jniLibs.srcDirs = ["@LINPHONESDK_BUILD_DIR@/libs-debug"]
        }
        release {
            root = 'build-types/release'
            jniLibs.srcDirs = ["@LINPHONESDK_BUILD_DIR@/libs"]
        }
    }
}

///////////// Task /////////////

task(releaseJavadoc, type: Javadoc, dependsOn: "assembleRelease") {
    source = srcDir
    excludes = javaExcludes.plus(['**/**.html', '**/**.aidl', '**/org/linphone/core/tools/**'])
    classpath += project.files(android.getBootClasspath().join(File.pathSeparator))
    //classpath += files(android.libraryVariants.release.javaCompile.classpath.files)
    classpath += configurations.javadocDeps
    options.encoding = 'UTF-8'
    options.addStringOption('Xdoclint:none', '-quiet')

    afterEvaluate {
        classpath += files(android.libraryVariants.collect { variant ->
            variant.javaCompileProvider.get().classpath.files
        })
    }
}

task sourcesJar(type: Jar) {
    classifier = 'sources'
    from android.sourceSets.main.java.srcDirs
}

task androidJavadocsJar(type: Jar, dependsOn: releaseJavadoc) {
    classifier = 'javadoc'
    from releaseJavadoc.destinationDir
}

task sdkZip(type: Zip) {
    from('linphone-sdk/bin/libs',
         'linphone-sdk/bin/outputs/aar')
    include '*'
    archiveName "linphone-sdk-android-@LINPHONESDK_VERSION@.zip"
}

task copyProguard(type: Copy) {
    from rootSdk + '/share/linphonej/'
    into "${buildDir}"
    include 'proguard.txt'
}

task copyAssets(type: Sync) {
    from rootSdk
    into "${buildDir}/sdk-assets/assets/org.linphone.core"
    include '**/*.png'
    include '**/*.pem'
    include '**/*.mkv'
    include '**/*.wav'
    include '**/*.jpg'
    include '**/*_grammar'

    //rename '(.*)', '$1'.toLowerCase()
    eachFile {
        path = path.toLowerCase() // To work around case insensitive fs (macosx)
        println("Syncing sdk asset ${sourcePath} to ${path}")
    }
    doFirst {
        println("Syncing sdk assets into root dir ${destinationDir}")
    }
    // do not copy those
    includeEmptyDirs = false
}

project.tasks['preBuild'].dependsOn 'copyAssets'
project.tasks['preBuild'].dependsOn 'copyProguard'
project.tasks['assemble'].dependsOn 'sourcesJar'
project.tasks['assemble'].dependsOn 'androidJavadocsJar'

afterEvaluate {
    def debugFile = file("$buildDir/outputs/aar/linphone-sdk-android.aar")
    tasks.named("assembleDebug").configure {
        doLast {
            debugFile.renameTo("$buildDir/outputs/aar/linphone-sdk-android-debug.aar")
        }
    }
    tasks.named("assembleRelease").configure {
        doLast {
            debugFile.renameTo("$buildDir/outputs/aar/linphone-sdk-android-release.aar")
        }
    }
}