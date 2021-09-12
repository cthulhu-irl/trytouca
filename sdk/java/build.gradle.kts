// Copyright 2021 Touca, Inc. Subject to Apache-2.0 License.

buildscript {
    repositories {
        mavenCentral()
    }
    dependencies {
        classpath(kotlin("gradle-plugin", version="1.4.30"))
    }
}

allprojects {
    repositories {
        mavenCentral()
    }
    group = "io.touca"
    version = "0.1.0"
    description = "Touca SDK for Java"
}
