// Copyright 2021 Touca, Inc. Subject to Apache-2.0 License.

plugins {
    `java-library`
    `maven-publish`
    jacoco
    pmd
    signing
    id("com.github.spotbugs") version "5.0.0-beta.1"
}

dependencies {
    implementation("com.google.code.gson:gson:2.8.8")
    implementation("com.google.flatbuffers:flatbuffers-java:2.0.3")
    implementation("commons-cli:commons-cli:1.4")
    testImplementation(platform("org.junit:junit-bom:5.8.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
}

tasks.test {
    useJUnitPlatform()
    testLogging {
        events("passed", "skipped", "failed")
    }
}

configure<JavaPluginConvention> {
    sourceCompatibility = JavaVersion.VERSION_1_8
    targetCompatibility = JavaVersion.VERSION_1_8
}

tasks.jar {
    manifest {
        attributes(mapOf("Implementation-Title" to project.name,
                         "Implementation-Version" to project.version,
                         "Implementation-Vendor" to "Touca, Inc."))
    }
}

java {
    withSourcesJar()
    withJavadocJar()
}

tasks {
    jacocoTestReport {
        reports {
            xml.isEnabled = true
            html.isEnabled = true
        }
    }

    check {
        dependsOn(jacocoTestReport)
    }

    javadoc {
        (options as StandardJavadocDocletOptions).apply {
            if (JavaVersion.current() >= JavaVersion.VERSION_1_9) {
                addBooleanOption("html5", true)
                setWindowTitle("Touca ${project.version}")
                setDocTitle("Touca ${project.version}")
            }
        }
    }

    pmd {
        isConsoleOutput = false
        toolVersion = "6.38.0"
        isIgnoreFailures = true
    }

    spotbugs {
        ignoreFailures.set(true)
        showProgress.set(false)
        showStackTraces.set(false)
    }

    withType<com.github.spotbugs.snom.SpotBugsTask> {
        enabled = baseName.equals("main")
        reports.maybeCreate("html").isEnabled = true
        reports.maybeCreate("xml").isEnabled = false
    }
}

configure<PublishingExtension> {
    publications {
        create<MavenPublication>("touca") {
            from(components["java"])
            afterEvaluate {
                pom {
                    name.set("${groupId}:${artifactId}")
                    description.set("Touca SDK for Java")
                    url.set("https://github.com/trytouca/touca-java.git")
                    licenses {
                        license {
                            name.set("Apache License 2.0")
                            url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                        }
                    }
                    organization {
                        name.set("Touca")
                        url.set("https://touca.io")
                    }
                    developers {
                        developer {
                            id.set("ghorbanzade")
                            name.set("Pejman Ghorbanzade")
                            email.set("pejman@touca.io")
                            organization.set("Touca")
                            organizationUrl.set("https://touca.io")
                            inceptionYear.set("2021")
                        }
                    }
                    scm {
                        url.set("https://github.com/trytouca/touca-java")
                        connection.set("scm:git:https://github.com/trytouca/touca-java.git")
                        developerConnection.set("scm:git:ssh://git@github.com:trytouca/touca-java.git")
                    }
                }
            }
        }
    }

    repositories {
        if (System.getenv("ORG_GRADLE_PROJECT_ossrhUsername") != null) {
            maven {
                name = "ossrh"
                credentials(PasswordCredentials::class)
                val releasesRepoUrl = "https://s01.oss.sonatype.org/service/local/staging/deploy/maven2"
                val snapshotsRepoUrl = "https://s01.oss.sonatype.org/content/repositories/snapshots"
                url = uri(if (version.toString().endsWith("SNAPSHOT")) snapshotsRepoUrl else releasesRepoUrl)
            }
        }
        maven {
            name = "build"
            url = uri(layout.buildDirectory.dir("repo"))
        }
    }

    signing {
        val signingKey: String? by project
        val signingKeyId: String? by project
        val signingPassword: String? by project
        if (signingKey != null && signingKey != "") {
            useInMemoryPgpKeys(signingKeyId, signingKey, signingPassword)
            sign(publishing.publications["touca"])
        }
    }
}
