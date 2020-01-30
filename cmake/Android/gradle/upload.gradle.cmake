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

apply plugin: 'maven-publish'

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
if (project.hasProperty("minimal-size")) {
    artefactGroupId = artefactGroupId + '.minimal'
}
println("AAR artefact group is: " + artefactGroupId + ", SDK version @LINPHONESDK_VERSION@")

publishing {
    publications {
        debug(MavenPublication) {
            groupId artefactGroupId
            artifactId 'linphone-sdk-android' + '-debug'
            version "@LINPHONESDK_VERSION@"
            artifact("$buildDir/outputs/aar/linphone-sdk-android-debug.aar")
        }
        release(MavenPublication) {
            groupId artefactGroupId
            artifactId 'linphone-sdk-android'
            version "@LINPHONESDK_VERSION@"
            artifact("$buildDir/outputs/aar/linphone-sdk-android-release.aar")
        }
    }
    repositories {
        maven {
            url "./maven_repository/"
        }
    }
}