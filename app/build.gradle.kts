import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// Release 签名信息存于 tool/keystore.properties（已 gitignore，含私钥口令）；缺失时 release 构建不签名
val releaseSigning = Properties().apply {
    val f = rootProject.file("tool/keystore.properties")
    if (f.exists()) f.inputStream().use { load(it) }
}.takeIf { it.getProperty("storeFile") != null }

android {
    namespace = "com.tvproxy"
    compileSdk = 35
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.tvproxy"
        minSdk = 21
        targetSdk = 29
        versionCode = 30
        versionName = "0.9.1"
        ndk {
            // -Pabi=arm64-v8a 只打指定 ABI；不传则三个都打（模拟器需要 x86_64）
            val abi = (project.findProperty("abi") as String?)
                ?.split(",")
                ?.map { it.trim() }
                ?.filter { it.isNotEmpty() }
            abiFilters += abi ?: listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        }
    }

    signingConfigs {
        create("release") {
            if (releaseSigning != null) {
                storeFile = rootProject.file(releaseSigning.getProperty("storeFile"))
                storePassword = releaseSigning.getProperty("storePassword")
                keyAlias = releaseSigning.getProperty("keyAlias")
                keyPassword = releaseSigning.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (releaseSigning != null) {
                signingConfig = signingConfigs.getByName("release")
            }
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    lint {
        // 侧载分发不走 Google Play：targetSdk 29 会被 lintVitalRelease 的 ExpiredTargetSdkVersion 拦
        checkReleaseBuilds = false
    }

    externalNativeBuild {
        ndkBuild {
            path = file("src/main/jni/Android.mk")
        }
    }
}

dependencies {
    implementation("androidx.leanback:leanback:1.2.0")
}
