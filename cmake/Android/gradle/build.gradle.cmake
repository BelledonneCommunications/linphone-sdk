// Project information
buildDir = 'linphone-sdk/bin'

buildscript {
    repositories {
        jcenter()
        mavenCentral()
        mavenLocal()
        google()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.1.0'
    }
}

allprojects {
    repositories {
        google()
        jcenter()
        mavenCentral()
        mavenLocal()
    }
}

configurations {
    javadocDeps
}



apply plugin: 'com.android.library'

dependencies {
    implementation 'org.apache.commons:commons-compress:1.16.1'
    javadocDeps 'org.apache.commons:commons-compress:1.16.1'
}

def rootSdk = '@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@'
def srcDir = ['@LINPHONESDK_DIR@/mediastreamer2/java/src']
srcDir += [rootSdk + '/share/linphonej/java/org/linphone/core/']
srcDir += ['@LINPHONESDK_DIR@/linphone/wrappers/java/classes/']

def excludePackage = []

excludePackage.add('**/gdb.*')
excludePackage.add('**/libopenh264**')
excludePackage.add('**/**tester**')
excludePackage.add('**/LICENSE.txt')

android {

    buildTypes {
        release {}
        debug {}
    }

    defaultConfig {
        compileSdkVersion 28
        buildToolsVersion "28.0.0"
        multiDexEnabled true
        setProperty("archivesBaseName", "linphone-sdk-android")
    }

    // Signing
    signingConfigs {
        release {
            storeFile file(RELEASE_STORE_FILE)
            storePassword RELEASE_STORE_PASSWORD
            keyAlias RELEASE_KEY_ALIAS
            keyPassword RELEASE_KEY_PASSWORD
        }
    }
    buildTypes {
        release {
            signingConfig signingConfigs.release
        }
        packaged {
            initWith release
            signingConfig null
            //matchingFallbacks = ['debug', 'release']
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
            jniLibs.srcDirs = ["@LINPHONESDK_BUILD_DIR@/libs"]
            //resources.srcDir("res")

            java.excludes = ['**/mediastream/MediastreamerActivity.java']

            // Exclude some useless files
            packagingOptions {
                excludes = excludePackage
            }
        }
        debug.setRoot('build-types/debug')
        release.setRoot('build-types/release')
    }
}

///////////// Task /////////////

task(releaseJavadoc, type: Javadoc, dependsOn: "assembleRelease") {
    source = srcDir
    excludes = ['**/mediastream/MediastreamerActivity.java',
                '**/**.html',
                '**/**.aidl']
    classpath += project.files(android.getBootClasspath().join(File.pathSeparator))
    classpath += files(android.libraryVariants.release.javaCompile.classpath.files)
    classpath += configurations.javadocDeps
    options.encoding = 'UTF-8'
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

task copyAssets(type: Sync) {
    from rootSdk
    into "${buildDir}/sdk-assets/assets/org.linphone.sdk"
    include '**/*.png'
    include '**/*.pem'
    include '**/*.mkv'
    include '**/*.wav'
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
