import java.io.File
import java.io.FileInputStream
import java.util.Properties

plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

// 发布签名: 从 android/key.properties 读取密钥信息 (该文件不入库, 见 android/.gitignore).
// 本地没有该文件时回退 debug 签名, 不影响 flutter run / flutter build 的日常开发;
// CI 会传 REQUIRE_RELEASE_SIGNING=1 强制要求密钥存在, 避免缺失时静默产出装不上的包.
val keystorePropertiesFile = rootProject.file("key.properties")
val keystoreProperties = Properties()
val hasReleaseKey = keystorePropertiesFile.exists()

if (hasReleaseKey) {
    FileInputStream(keystorePropertiesFile).use { keystoreProperties.load(it) }

    val requiredFields = listOf("storeFile", "storePassword", "keyAlias", "keyPassword")
    val missingFields = requiredFields.filter { keystoreProperties.getProperty(it).isNullOrBlank() }
    if (missingFields.isNotEmpty()) {
        throw GradleException(
            "${keystorePropertiesFile.absolutePath} 缺少字段: ${missingFields.joinToString()}"
        )
    }

    val storeFilePath = keystoreProperties.getProperty("storeFile")
    if (!File(storeFilePath).exists()) {
        throw GradleException(
            "${keystorePropertiesFile.absolutePath} 中的 storeFile 不存在: $storeFilePath"
        )
    }
}

if (System.getenv("REQUIRE_RELEASE_SIGNING") == "1" && !hasReleaseKey) {
    throw GradleException(
        "REQUIRE_RELEASE_SIGNING=1 但未找到 ${keystorePropertiesFile.absolutePath}, " +
            "拒绝用 debug 密钥生成 release 包; 请先在该文件里配置正式密钥."
    )
}

android {
    namespace = "com.jackarain.xproxy"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.jackarain.xproxy"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        // VpnService.protect(int) 需要 API 22+, 这里直接使用 23.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    signingConfigs {
        // key.properties 的 storeFile 需用绝对路径: file() 以 app 模块目录为基准.
        if (hasReleaseKey) {
            create("release") {
                storeFile = file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            // 有正式密钥用正式密钥; 缺失时(本地日常开发)回退 debug, 保证 flutter run --release 可用.
            signingConfig = if (hasReleaseKey) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
            // SWIG/JNI 类不可混淆, 否则 native RegisterNatives 会找不到方法.
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
}

flutter {
    source = "../.."
}
