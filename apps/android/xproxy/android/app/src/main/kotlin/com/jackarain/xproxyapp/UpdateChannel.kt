package com.jackarain.xproxyapp

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.content.FileProvider
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.security.MessageDigest
import java.util.concurrent.Executors
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

/**
 * 更新包的解压/校验/安装 (Flutter 侧下载完成后调用).
 *
 * 解压要写出数十兆数据, 放在工作线程执行; 而申请「安装未知应用」设置页与
 * 调起系统安装器都属于界面操作, 必须回到主线程.
 */
class UpdateChannel(private val activity: Activity) {
    companion object {
        private const val CHANNEL = "com.jackarain.xproxy/update"
        private const val APK_MIME = "application/vnd.android.package-archive"
        private const val UPDATE_DIR = "update"
        private const val APK_NAME = "app-release.apk"
        private const val APK_SUFFIX = ".apk"
    }

    private val worker = Executors.newSingleThreadExecutor()

    fun attach(engine: FlutterEngine) {
        MethodChannel(engine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "download_dir" -> result.success(updateDir().absolutePath)
                    "current_version" -> {
                        try {
                            result.success(infoMap(packageInfo(activity.packageName)))
                        } catch (e: Throwable) {
                            result.error("UPDATE_FAILED", e.message, null)
                        }
                    }
                    // 解压出 apk 并读取其版本/签名信息(不安装).
                    "inspect_zip" -> inWorker(result) {
                        val apk = extractApk(call.argument<String>("zip").orEmpty())
                        infoMap(packageInfo(apk.absolutePath)) +
                            mapOf("apkPath" to apk.absolutePath)
                    }
                    "install" -> install(call.argument<String>("apk").orEmpty(), result)
                    else -> result.notImplemented()
                }
            }
    }

    /** 在工作线程执行 [block], 结果与异常都切回主线程回传. */
    private fun inWorker(result: MethodChannel.Result, block: () -> Any) {
        worker.execute {
            try {
                val value = block()
                activity.runOnUiThread { result.success(value) }
            } catch (e: Throwable) {
                activity.runOnUiThread {
                    result.error("UPDATE_FAILED", e.message ?: e.toString(), null)
                }
            }
        }
    }

    /**
     * 校验更新包并调起系统安装器.
     *
     * 返回 `started` 表示安装器已调起; `need_permission` 表示需要先授予
     * 「安装未知应用」权限 (设置页已打开), 授权后可重复调用.
     */
    private fun install(apkPath: String, result: MethodChannel.Result) {
        worker.execute {
            val plan = runCatching { planInstall(apkPath) }
            activity.runOnUiThread {
                plan.fold(
                    onSuccess = { apk ->
                        try {
                            if (needsInstallPermission()) {
                                openUnknownSourcesSettings()
                                result.success("need_permission")
                            } else {
                                launchInstaller(apk)
                                result.success("started")
                            }
                        } catch (e: Throwable) {
                            result.error("UPDATE_FAILED", e.message ?: e.toString(), null)
                        }
                    },
                    onFailure = { e ->
                        result.error("UPDATE_FAILED", e.message ?: e.toString(), null)
                    },
                )
            }
        }
    }

    /** 校验待安装 apk: 必须位于更新目录且签名与当前应用一致. */
    private fun planInstall(apkPath: String): File {
        val apk = File(apkPath)
        if (!apk.isFile) throw IllegalStateException("更新包不存在")
        if (apk.canonicalFile.parentFile != updateDir().canonicalFile) {
            throw IllegalStateException("更新包路径不合法")
        }
        val remote = signerSha256(packageInfo(apk.absolutePath))
        if (remote.isEmpty()) throw IllegalStateException("无法读取更新包签名")
        val local = signerSha256(packageInfo(activity.packageName))
        // 签名不一致时系统必然拒绝覆盖安装, 提前失败以便 Flutter 侧说明原因.
        if (local.isNotEmpty() && remote != local) {
            throw IllegalStateException("更新包签名与当前应用不一致")
        }
        return apk
    }

    /** 解压更新包中的 apk 到更新目录, 解压成功后删除压缩包. */
    private fun extractApk(zipPath: String): File {
        val zip = File(zipPath)
        if (!zip.isFile) throw IllegalStateException("更新包不存在")
        val apk = File(updateDir(), APK_NAME)
        ZipFile(zip).use { archive ->
            val entry = firstApkEntry(archive)
                ?: throw IllegalStateException("更新包内未找到 apk")
            archive.getInputStream(entry).use { input ->
                apk.outputStream().use { output -> input.copyTo(output) }
            }
        }
        if (packageInfo(apk.absolutePath) == null) {
            apk.delete()
            throw IllegalStateException("更新包内的 apk 无法解析")
        }
        zip.delete()
        return apk
    }

    private fun firstApkEntry(archive: ZipFile): ZipEntry? {
        val entries = archive.entries()
        while (entries.hasMoreElements()) {
            val entry = entries.nextElement()
            if (!entry.isDirectory && entry.name.endsWith(APK_SUFFIX, true)) {
                return entry
            }
        }
        return null
    }

    /** 更新包目录: 应用私有外部目录(空间充足), 不可用时退回内部 cache. */
    private fun updateDir(): File {
        val dir = activity.getExternalFilesDir(UPDATE_DIR)
            ?: File(activity.cacheDir, UPDATE_DIR)
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    @Suppress("DEPRECATION")
    private fun packageInfo(path: String): PackageInfo? {
        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            PackageManager.GET_SIGNING_CERTIFICATES
        } else {
            PackageManager.GET_SIGNATURES
        }
        return activity.packageManager.getPackageArchiveInfo(path, flags)
    }

    @Suppress("DEPRECATION")
    private fun signerSha256(info: PackageInfo?): String {
        if (info == null) return ""
        val signers = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            info.signingInfo?.apkContentsSigners
        } else {
            info.signatures
        }
        val signer = signers?.firstOrNull() ?: return ""
        val digest = MessageDigest.getInstance("SHA-256").digest(signer.toByteArray())
        return digest.joinToString("") { "%02x".format(it) }
    }

    private fun infoMap(info: PackageInfo?): Map<String, Any> {
        val code = when {
            info == null -> 0L
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P -> info.longVersionCode
            else -> info.versionCode.toLong()
        }
        return mapOf(
            "versionCode" to code,
            "versionName" to (info?.versionName ?: ""),
            "signerSha256" to signerSha256(info),
        )
    }

    private fun needsInstallPermission(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false
        return !activity.packageManager.canRequestPackageInstalls()
    }

    /** 打开本应用的「安装未知应用」授权页. */
    private fun openUnknownSourcesSettings() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val intent = Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES).setData(
            Uri.parse("package:${activity.packageName}")
        )
        activity.startActivity(intent)
    }

    /** 经 FileProvider 把更新包交给系统安装器, 避免暴露 file:// URI. */
    private fun launchInstaller(apk: File) {
        val uri = FileProvider.getUriForFile(
            activity, "${activity.packageName}.fileprovider", apk
        )
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, APK_MIME)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        activity.startActivity(intent)
    }
}
