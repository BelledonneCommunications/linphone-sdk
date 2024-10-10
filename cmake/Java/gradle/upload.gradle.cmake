// Project information
buildDir = 'linphone-sdk/bin/libs'

buildscript {
    repositories {
        mavenCentral()
        maven {
            url "https://plugins.gradle.org/m2/"
        }
    }
}

apply plugin: 'maven-publish'

publishing {
    publications {
        release(MavenPublication) {
            groupId 'org.linphone'
            artifactId 'linphone-sdk'
            version "@LINPHONESDK_VERSION@"
            artifact("$buildDir/linphone-sdk.jar")
            artifact source: "$buildDir/linphone-sdk-sources.jar", classifier: 'sources', extension: 'jar'
            artifact source: "$buildDir/linphone-sdk-javadoc.jar", classifier: 'javadoc', extension: 'jar'

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