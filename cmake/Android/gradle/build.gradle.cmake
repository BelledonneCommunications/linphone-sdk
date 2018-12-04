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
        maven { url "https://raw.github.com/synergian/wagon-git/releases"}
    }
}

configurations {
    javadocDeps
    deployerJars
}

apply plugin: 'com.android.library'
apply plugin: 'maven'

dependencies {
    implementation 'org.apache.commons:commons-compress:1.16.1'
    javadocDeps 'org.apache.commons:commons-compress:1.16.1'
    deployerJars "ar.com.synergian:wagon-git:0.2.5"
}

def rootSdk = '@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@'
def srcDir = ['@LINPHONESDK_DIR@/mediastreamer2/java/src']
srcDir += [rootSdk + '/share/linphonej/java/org/linphone/core/']
srcDir += ['@LINPHONESDK_DIR@/linphone/wrappers/java/classes/']

def excludePackage = []

excludePackage.add('**/gdb.*')
excludePackage.add('**/libopenh264**')
excludePackage.add('**/LICENSE.txt')

def gitVersion = new ByteArrayOutputStream()
def gitBranch = new ByteArrayOutputStream()

task getGitVersion {
    exec {
        commandLine 'git', 'describe', '--always'
        standardOutput = gitVersion
    }
    exec {
        commandLine 'git', 'name-rev', '--name-only', 'HEAD'
        standardOutput = gitBranch
    }
    doLast {
        def branchSplit = gitBranch.toString().trim().split('/')
        def splitLen = branchSplit.length
        if (splitLen == 4) {
            gitBranch = branchSplit[2] + '/' + branchSplit[3]
            println("Local repository seems to be in detached head state, using last 2 segments of Git branch: " + gitBranch.toString().trim())
        } else {
            println("Git branch: " + gitBranch.toString().trim())
        }
    }
}

project.tasks['preBuild'].dependsOn 'getGitVersion'

android {

    buildTypes {
        release {}
        debug {}
    }

    defaultConfig {
        compileSdkVersion 28
        buildToolsVersion "28.0.0"
        minSdkVersion 16
        targetSdkVersion 28
        versionCode 4100
        versionName "4.1"
        multiDexEnabled true
        setProperty("archivesBaseName", "linphone-sdk-android")
        consumerProguardFiles "${buildDir}/proguard.txt"
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
            minifyEnabled true
            useProguard true
            proguardFiles "${buildDir}/proguard.txt"
            resValue "string", "linphone_sdk_version", gitVersion.toString().trim()
            resValue "string", "linphone_sdk_branch", gitBranch.toString().trim()
        }
        packaged {
            initWith release
            signingConfig null
            //matchingFallbacks = ['debug', 'release']
        }
        debug {
            debuggable true
            resValue "string", "linphone_sdk_version", gitVersion.toString().trim() + "-debug"
            resValue "string", "linphone_sdk_branch", gitBranch.toString().trim()
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

uploadArchives {
    repositories {
        mavenDeployer {
            configuration = configurations.deployerJars
            repository(url: 'git:' + gitBranch.toString().trim() + '://git@gitlab.linphone.org:BC/public/maven_repository.git')
            pom.project {
                groupId 'org.linphone'
                artifactId 'linphone-sdk-android'
                version gitVersion.toString().trim()
            }
        }
    }
}

project.tasks['uploadArchives'].dependsOn 'getGitVersion'
