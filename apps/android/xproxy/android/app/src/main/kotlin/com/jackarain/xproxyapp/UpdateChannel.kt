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

/**
 * 自更新相关的原生能力: 读取已安装/已下载 APK 的版本与签名, 调起系统安装器.
 *
 * 读取 APK 信息与签名校验要扫数十兆文件, 放在工作线程; 申请「安装未知应用」
 * 设置页与调起系统安装器属于界面操作, 必须回到主线程.
 */
class UpdateChannel(private val activity: Activity) {
    companion object {
        private const val CHANNEL = "com.jackarain.xproxy/update"
        private const val APK_MIME = "application/vnd.android.package-archive"
        private const val UPDATE_DIR = "update"
    }

    private val worker = Executors.newSingleThreadExecutor()

    /** 引擎销毁时回收工作线程 (已提交的任务继续跑完). */
    fun close() {
        worker.shutdown()
    }

    fun attach(engine: FlutterEngine) {
        MethodChannel(engine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    // 下载落地目录: 应用私有外部目录(空间充足), 不可用时退回内部 cache.
                    "update_dir" -> result.success(updateDir().absolutePath)
                    // 已安装 APK 的整文件 SHA-1: 与下载站给出的校验值比对判断有无更新.
                    "installed_apk_hash" -> inWorker(result) {
                        sha1Hex(File(activity.applicationInfo.sourceDir))
                    }
                    "current_version" -> {
                        try {
                            val info = installedPackageInfo()
                                ?: throw IllegalStateException("无法读取已安装应用信息")
                            result.success(infoMap(info))
                        } catch (e: Throwable) {
                            result.error("UPDATE_FAILED", e.message, null)
                        }
                    }
                    // 读取已下载 APK 的版本/签名/校验值(不安装), 供 Flutter 侧校验与提示.
                    "inspect_apk" -> inWorker(result) {
                        val apk = updateDirApk(call.argument<String>("apk").orEmpty())
                        infoMap(archivePackageInfo(apk.absolutePath)) +
                            mapOf("sha1" to sha1Hex(apk))
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

    /** 待安装 APK 必须是更新目录下的文件, 防止被引导去安装任意路径. */
    private fun updateDirApk(apkPath: String): File {
        val apk = File(apkPath)
        if (!apk.isFile) throw IllegalStateException("更新包不存在")
        if (apk.canonicalFile.parentFile != updateDir().canonicalFile) {
            throw IllegalStateException("更新包路径不合法")
        }
        return apk
    }

    /** 校验待安装 apk: 签名必须与当前应用一致, 否则系统必然拒绝覆盖安装. */
    private fun planInstall(apkPath: String): File {
        val apk = updateDirApk(apkPath)
        val remote = signerSha256(archivePackageInfo(apk.absolutePath))
        if (remote.isEmpty()) throw IllegalStateException("无法读取更新包签名")
        val local = signerSha256(installedPackageInfo())
        if (local.isEmpty()) throw IllegalStateException("无法读取当前应用签名")
        if (remote != local) {
            throw IllegalStateException("更新包签名与当前应用不一致")
        }
        return apk
    }

    private fun updateDir(): File {
        val dir = activity.getExternalFilesDir(UPDATE_DIR)
            ?: File(activity.cacheDir, UPDATE_DIR)
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    /** 读取签名所需的 flags: API 28+ 用 GET_SIGNING_CERTIFICATES, 更早用 GET_SIGNATURES. */
    @Suppress("DEPRECATION")
    private fun signatureFlags(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            PackageManager.GET_SIGNING_CERTIFICATES
        } else {
            PackageManager.GET_SIGNATURES
        }

    /** 解析 APK 归档文件: 只接受文件路径, 传包名会解析失败返回 null. */
    @Suppress("DEPRECATION")
    private fun archivePackageInfo(path: String): PackageInfo? =
        activity.packageManager.getPackageArchiveInfo(path, signatureFlags())

    /** 已安装应用自身的信息: 必须走 getPackageInfo, getPackageArchiveInfo 无法按包名查询. */
    @Suppress("DEPRECATION")
    private fun installedPackageInfo(): PackageInfo? =
        activity.packageManager.getPackageInfo(activity.packageName, signatureFlags())

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

    /** 整文件 SHA-1 (小写十六进制), 与下载站返回的校验值同一算法. */
    private fun sha1Hex(file: File): String {
        val digest = MessageDigest.getInstance("SHA-1")
        val buffer = ByteArray(1 shl 20)
        file.inputStream().use { input ->
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                digest.update(buffer, 0, read)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
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
