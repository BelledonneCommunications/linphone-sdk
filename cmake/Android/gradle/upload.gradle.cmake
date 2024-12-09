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
        classpath 'com.android.tools.build:gradle:8.0.0'
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
            artifact source: "$buildDir/libs/linphone-sdk-android-sources.jar", classifier: 'sources', extension: 'jar'
            artifact source: "$buildDir/libs/linphone-sdk-android-javadoc.jar", classifier: 'javadoc', extension: 'jar'
            artifact source: "$buildDir/distributions/linphone-sdk-android-libs-debug.zip", classifier: 'libs-debug', extension: 'zip'

            pom {
                name = 'Linphone'
                description = 'Instant messaging and voice/video over IP (VoIP) library'
                url = 'https://linphone.org/'
                licenses {
                    license {
                        name = 'GNU GENERAL PUBLIC LICENSE, Version 3.0'
                        url = 'https://www.gnu.org/licenses/gpl-3.0.en.html'
                    }
                }
                scm {
                    connection = 'scm:git:https://gitlab.linphone.org/BC/public/linphone-sdk.git'
                    url = 'https://gitlab.linphone.org/BC/public/linphone-sdk'
                }
            }
        }

        release(MavenPublication) {
            groupId artefactGroupId
            artifactId 'linphone-sdk-android'
            version "@LINPHONESDK_VERSION@"
            artifact("$buildDir/outputs/aar/linphone-sdk-android-release.aar")
            artifact source: "$buildDir/libs/linphone-sdk-android-sources.jar", classifier: 'sources', extension: 'jar'
            artifact source: "$buildDir/libs/linphone-sdk-android-javadoc.jar", classifier: 'javadoc', extension: 'jar'
            artifact source: "$buildDir/distributions/linphone-sdk-android-libs-debug.zip", classifier: 'libs-debug', extension: 'zip'

            pom {
                name = 'Linphone'
                description = 'Instant messaging and voice/video over IP (VoIP) library'
                url = 'https://linphone.org/'
                licenses {
                    license {
                        name = 'GNU GENERAL PUBLIC LICENSE, Version 3.0'
                        url = 'https://www.gnu.org/licenses/gpl-3.0.en.html'
                    }
                }
                scm {
                    connection = 'scm:git:https://gitlab.linphone.org/BC/public/linphone-sdk.git'
                    url = 'https://gitlab.linphone.org/BC/public/linphone-sdk'
                }
            }
        }
    }
    
    repositories {
        maven {
            url "./maven_repository/"
        }
    }
}