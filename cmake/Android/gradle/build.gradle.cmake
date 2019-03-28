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
        classpath 'com.android.tools.build:gradle:3.3.2'
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
apply plugin: 'maven-publish'

dependencies {
    implementation 'org.apache.commons:commons-compress:1.16.1'
    javadocDeps 'org.apache.commons:commons-compress:1.16.1'
}

static def isGeneratedJavaWrapperAvailable() {
    File coreWrapper = new File('@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@/share/linphonej/java/org/linphone/core/Core.java')
    return coreWrapper.exists()
}

def rootSdk = '@LINPHONESDK_BUILD_DIR@/linphone-sdk/android-@LINPHONESDK_FIRST_ARCH@'
def srcDir = ['@LINPHONESDK_DIR@/mediastreamer2/java/src']
srcDir += [rootSdk + '/share/linphonej/java/org/linphone/core/']
srcDir += ['@LINPHONESDK_DIR@/linphone/wrappers/java/classes/']

def excludePackage = []
excludePackage.add('**/gdb.*')
excludePackage.add('**/libopenh264**')
excludePackage.add('**/LICENSE.txt')

def javaExcludes = []
javaExcludes.add('**/mediastream/MediastreamerActivity.java')
if (!isGeneratedJavaWrapperAvailable()) {
    // We have to remove some classes that requires the new java wrapper
    println("Old java wrapper detected, removing Utils and H264Helper classes from AAR")
    javaExcludes.add("**/Utils.java")
    javaExcludes.add("**/H264Helper.java")

    // Add the previous wrapper to sources
    srcDir += ['@LINPHONESDK_DIR@/linphone/java/common/']
    srcDir += ['@LINPHONESDK_DIR@/linphone/java/impl/']
}

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
    defaultConfig {
        compileSdkVersion 28
        minSdkVersion 16
        targetSdkVersion 28
        versionCode 4100
        versionName "4.1"
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
        release {
            signingConfig signingConfigs.release
            minifyEnabled true
            useProguard true
            proguardFiles "${buildDir}/proguard.txt"
            resValue "string", "linphone_sdk_version", gitVersion.toString().trim()
            resValue "string", "linphone_sdk_branch", gitBranch.toString().trim()
        }
        debug {
            minifyEnabled false
            useProguard false
            debuggable true
            jniDebuggable true
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
    excludes = javaExcludes.plus(['**/**.html', '**/**.aidl'])
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

def artefactGroupId = 'org.linphone'
if (project.hasProperty("legacy-wrapper")) {
    artefactGroupId = artefactGroupId + '.legacy'
}
if (project.hasProperty("tunnel")) {
    artefactGroupId = artefactGroupId + '.tunnel'
}
if (project.hasProperty("no-video")) {
    artefactGroupId = artefactGroupId + '.no-video'
}
println("AAR artefact group id will be: " + artefactGroupId)

publishing {
    publications {
        debug(MavenPublication) {
            groupId artefactGroupId
            artifactId 'linphone-sdk-android' + '-debug'
            version gitVersion.toString().trim()
            artifact("$buildDir/outputs/aar/linphone-sdk-android-debug.aar")
        }
        release(MavenPublication) {
            groupId artefactGroupId
            artifactId 'linphone-sdk-android'
            version gitVersion.toString().trim()
            artifact("$buildDir/outputs/aar/linphone-sdk-android-release.aar")

            // Also upload the javadoc
            artifact androidJavadocsJar
        }
    }
    repositories {
        maven {
            url "./maven_repository/"
        }
    }
}

project.tasks['assemble'].dependsOn 'getGitVersion'
project.tasks['publish'].dependsOn 'assemble'