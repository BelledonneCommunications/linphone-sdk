plugins {
    id "com.google.osdetector" version "1.7.3"
    id "maven-publish"
}

repositories {
    mavenCentral()
    maven {
        url "https://plugins.gradle.org/m2/"
    }
}

// Define the output directory of the build (jars)
def buildDir = 'linphone-sdk/bin/libs'
def os = "${osdetector.os}"
// Determine OS classifier for artifact publishing:
// - For Linux: Uses distribution ID + version (e.g. "ubuntu.24.04")
// - For Rocky Linux: Only keeps major version (e.g. "rocky.9" from "9.4")
// - For other OSes: Uses simple OS name (e.g. "windows", "mac")
def osClassifier = os == "linux" ?
                        (osdetector.release.id == "rocky" ?
                        "${osdetector.release.id}.${osdetector.release.version.split('\\.')[0]}" :
                        "${osdetector.release.id}.${osdetector.release.version}") :
                        os

println "Detected OS classifier for this upload : ${osClassifier}"

publishing {
    publications {
        release(MavenPublication) {
            groupId = 'org.linphone'
            artifactId = 'linphone-sdk'
            version = "@LINPHONESDK_VERSION@"

           artifact("${buildDir}/linphone-sdk.jar") {
                classifier = osClassifier
            }
            artifact("${buildDir}/linphone-sdk-sources.jar") {
                classifier = 'sources'
                extension = 'jar'
            }
            artifact("${buildDir}/linphone-sdk-javadoc.jar") {
                classifier = 'javadoc'
                extension = 'jar'
            }

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
            url './maven_repository/'
        }
    }
}