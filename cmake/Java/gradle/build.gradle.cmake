plugins {
    id 'java'
    id 'java-library'
}

repositories {
    mavenCentral()
}

configurations {
    javadocDeps
}

dependencies {
    implementation 'org.apache.commons:commons-compress:1.25.0'
    javadocDeps 'org.apache.commons:commons-compress:1.25.0'
}

// Project information
buildDir = 'linphone-sdk/bin'
archivesBaseName = 'linphone-sdk'

def rootSdk = '@LINPHONESDK_BUILD_DIR@/linphone-sdk/java'
def srcDir = ['@LINPHONESDK_DIR@/mediastreamer2/java/src']
def pluginsDir = rootSdk + '/lib/mediastreamer/plugins/'
srcDir += [rootSdk + '/share/linphonej/java/']
srcDir += ['@LINPHONESDK_DIR@/liblinphone/wrappers/java/classes/']

def excludePackage = []
excludePackage.add('**/gdb.*')
excludePackage.add('**/libopenh264**')
excludePackage.add('**/LICENSE.txt')

def javaExcludes = []
javaExcludes.add('**/android/server/os/**')
javaExcludes.add('**/linphone/core/tools/audio/**')
javaExcludes.add('**/linphone/core/tools/compatibility/**')
javaExcludes.add('**/linphone/core/tools/firebase/**')
javaExcludes.add('**/linphone/core/tools/network/**')
javaExcludes.add('**/linphone/core/tools/receiver/**')
javaExcludes.add('**/linphone/core/tools/service/**')
javaExcludes.add('**/linphone/core/tools/AndroidPlatformHelper.java')
javaExcludes.add('**/linphone/core/tools/PushNotificationUtils.java')
javaExcludes.add('**/mediastream/video/**')
javaExcludes.add('**/mediastream/AACFilter.java')
javaExcludes.add('**/mediastream/MediastreamerAndroidContext.java')
javaExcludes.add('**/mediastream/Version.java')

def pluginsList = ""

task listPlugins() {
    fileTree(pluginsDir).visit { FileVisitDetails details -> 
        println("Found plugin: " + details.file.name)
        pluginsList = pluginsList + "\"" + details.file.name  + "\","
    }
}

project.tasks['compileJava'].dependsOn 'listPlugins'

task copyAssets(type: Sync) {
    from rootSdk
    into "${buildDir}/sdk-assets/assets/org.linphone.core"
    exclude '**/doc/*'
    include '**/*.png'
    include '**/*.pem'
    include '**/*.mkv'
    include '**/*.wav'
    include '**/*.jpg'
    include '**/*_grammar.belr'

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

task javadocsJar(type: Jar, dependsOn: javadoc) {
    archiveClassifier = 'javadoc'
    from javadoc.destinationDir
}

task sourcesJar(type: Jar) {
    archiveClassifier = 'sources'
    from sourceSets.main.allJava
    finalizedBy javadocsJar
}

project.tasks['processResources'].dependsOn 'copyAssets'
project.tasks['processResources'].dependsOn 'sourcesJar'
project.tasks['sourcesJar'].dependsOn 'copyAssets'

task sdkZip(type: Zip) {
    from('linphone-sdk/bin/libs')
    include '*'
    archiveFileName = "linphone-sdk-java-@LINPHONESDK_VERSION@.zip"
}

sourceSets {
    main {
        java.srcDirs = srcDir
        java.excludes = javaExcludes
        resources.srcDirs = ["${buildDir}/sdk-assets", "@LINPHONESDK_BUILD_DIR@/libs/"]
    }
}

java {
    toolchain {
        // Required for javadoc task...
        languageVersion.set(JavaLanguageVersion.of(21))
    }
}

javadoc {
    destinationDir = file("${buildDir}/docs/javadoc")
    exclude javaExcludes.plus(['**/**.html', '**/**.aidl', '**/org/linphone/core/tools/**'])
    options.encoding = 'UTF-8'
    options.addStringOption('Xdoclint:none', '-quiet')
    options.setOverview(rootSdk + '/share/linphonej/java/src/main/javadoc/overview.html')
}

jar {
    processResources.exclude('**/*.so.*', '**/*.a', '**/cmake', '**/pkgconfig')
}
