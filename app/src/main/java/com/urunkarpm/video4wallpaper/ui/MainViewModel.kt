package com.urunkarpm.video4wallpaper.ui

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.room.Room
import com.urunkarpm.video4wallpaper.data.db.AppDatabase
import com.urunkarpm.video4wallpaper.data.db.VideoEntity
import com.urunkarpm.video4wallpaper.data.pref.SettingsRepository
import com.urunkarpm.video4wallpaper.utils.FileManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL

sealed interface UpdateState {
    data object Idle : UpdateState
    data object Checking : UpdateState
    data class UpdateAvailable(val version: String, val downloadUrl: String, val releaseNotes: String) : UpdateState
    data class UpToDate(val version: String) : UpdateState
    data class Downloading(val progressPercent: Int, val bytesDownloaded: Long, val totalBytes: Long) : UpdateState
    data class Downloaded(val apkUri: Uri, val version: String) : UpdateState
    data class Error(val message: String) : UpdateState
}

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val db = Room.databaseBuilder(application, AppDatabase::class.java, "wallpaper_db").build()
    private val videoDao = db.videoDao()
    val settingsRepository = SettingsRepository(application)

    val currentVersion: String = runCatching {
        application.packageManager.getPackageInfo(application.packageName, 0).versionName
    }.getOrNull() ?: "1.0.0"

    private val _updateState = MutableStateFlow<UpdateState>(UpdateState.Idle)
    val updateState: StateFlow<UpdateState> = _updateState.asStateFlow()

    val videos: StateFlow<List<VideoEntity>> = videoDao.getAllVideos()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val selectedUri: StateFlow<String?> = settingsRepository.selectedVideoUri
        .stateIn(viewModelScope, SharingStarted.Eagerly, null)

    val loopEnabled: StateFlow<Boolean> = settingsRepository.loopEnabled
        .stateIn(viewModelScope, SharingStarted.Eagerly, true)

    val pauseOnBatterySaver: StateFlow<Boolean> = settingsRepository.pauseOnBatterySaver
        .stateIn(viewModelScope, SharingStarted.Eagerly, true)

    fun importVideo(uri: Uri) {
        viewModelScope.launch {
            val file = FileManager.copyUriToInternalStorage(getApplication(), uri)
            if (file != null) {
                val entity = VideoEntity(
                    uri = file.absolutePath,
                    fileName = file.name,
                    fileSizeBytes = file.length(),
                    durationMs = 0L
                )
                videoDao.insertVideo(entity)
                settingsRepository.setSelectedVideoUri(file.absolutePath)
            }
        }
    }

    fun selectVideo(uri: String) {
        viewModelScope.launch {
            settingsRepository.setSelectedVideoUri(uri)
        }
    }

    fun setLoopEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.setLoopEnabled(enabled)
        }
    }

    fun setPauseOnBatterySaver(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.setPauseOnBatterySaver(enabled)
        }
    }

    fun deleteVideo(video: VideoEntity) {
        viewModelScope.launch {
            videoDao.deleteVideo(video)
            runCatching {
                val file = File(video.uri)
                if (file.exists() && file.isFile) {
                    file.delete()
                }
            }
            if (selectedUri.value == video.uri) {
                val remaining = videos.value.filter { it.id != video.id }
                settingsRepository.setSelectedVideoUri(remaining.firstOrNull()?.uri ?: "")
            }
        }
    }

    // ponytail: Direct HttpURLConnection + FileProvider in-app download cuts out heavy auto-update SDKs.
    // Ceiling: Requires user confirmation prompt via system package installer.
    // Upgrade path: PackageInstaller session API for silent device-owner updates if built as MDM enterprise app.
    fun checkForUpdates() {
        viewModelScope.launch(Dispatchers.IO) {
            _updateState.value = UpdateState.Checking
            try {
                val url = URL("https://api.github.com/repos/urunkarpm/Video4Wallpaper/releases/latest")
                val connection = (url.openConnection() as HttpURLConnection).apply {
                    connectTimeout = 10000
                    readTimeout = 10000
                    setRequestProperty("Accept", "application/vnd.github.v3+json")
                    setRequestProperty("User-Agent", "Video4Wallpaper-App")
                }

                if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                    _updateState.value = UpdateState.Error("Server returned code ${connection.responseCode}")
                    return@launch
                }

                val response = connection.inputStream.bufferedReader().use { it.readText() }
                val json = JSONObject(response)
                val tagName = json.optString("tag_name", "")
                val notes = json.optString("body", "").trim()
                val assets = json.optJSONArray("assets")

                var apkDownloadUrl: String? = null
                if (assets != null) {
                    for (i in 0 until assets.length()) {
                        val asset = assets.getJSONObject(i)
                        val name = asset.optString("name", "")
                        if (name.endsWith(".apk", ignoreCase = true)) {
                            apkDownloadUrl = asset.optString("browser_download_url", "")
                            break
                        }
                    }
                }

                if (apkDownloadUrl.isNullOrEmpty()) {
                    _updateState.value = UpdateState.Error("No APK release asset found in $tagName")
                    return@launch
                }

                if (isNewerVersion(tagName, currentVersion)) {
                    _updateState.value = UpdateState.UpdateAvailable(
                        version = tagName,
                        downloadUrl = apkDownloadUrl,
                        releaseNotes = notes
                    )
                } else {
                    _updateState.value = UpdateState.UpToDate(tagName)
                }
            } catch (e: Exception) {
                _updateState.value = UpdateState.Error(e.localizedMessage ?: "Failed to check for updates")
            }
        }
    }

    fun downloadUpdate(downloadUrl: String, version: String) {
        viewModelScope.launch(Dispatchers.IO) {
            _updateState.value = UpdateState.Downloading(0, 0, 0)
            try {
                var currentUrl = downloadUrl
                var connection: HttpURLConnection
                var redirects = 0
                while (true) {
                    connection = (URL(currentUrl).openConnection() as HttpURLConnection).apply {
                        instanceFollowRedirects = true
                        connectTimeout = 15000
                        readTimeout = 30000
                        setRequestProperty("User-Agent", "Video4Wallpaper-App")
                    }
                    val code = connection.responseCode
                    if (code in 300..399) {
                        val location = connection.getHeaderField("Location") ?: break
                        connection.disconnect()
                        currentUrl = location
                        redirects++
                        if (redirects > 5) break
                    } else {
                        break
                    }
                }

                val totalLength = connection.contentLengthLong
                val updateDir = File(getApplication<Application>().cacheDir, "updates").apply {
                    if (!exists()) mkdirs()
                }
                val apkFile = File(updateDir, "Video4Wallpaper-$version.apk")
                if (apkFile.exists()) apkFile.delete()

                connection.inputStream.use { input ->
                    FileOutputStream(apkFile).use { output ->
                        val buffer = ByteArray(8192)
                        var bytesRead: Int
                        var downloaded = 0L
                        var lastPercent = 0
                        while (input.read(buffer).also { bytesRead = it } != -1) {
                            output.write(buffer, 0, bytesRead)
                            downloaded += bytesRead
                            val percent = if (totalLength > 0) ((downloaded * 100) / totalLength).toInt() else 0
                            if (percent != lastPercent || downloaded == totalLength) {
                                lastPercent = percent
                                _updateState.value = UpdateState.Downloading(percent, downloaded, totalLength)
                            }
                        }
                    }
                }

                val apkUri = FileProvider.getUriForFile(
                    getApplication(),
                    "${getApplication<Application>().packageName}.provider",
                    apkFile
                )
                _updateState.value = UpdateState.Downloaded(apkUri, version)
            } catch (e: Exception) {
                _updateState.value = UpdateState.Error(e.localizedMessage ?: "Download failed")
            }
        }
    }

    fun installUpdate(context: Context, apkUri: Uri) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (!context.packageManager.canRequestPackageInstalls()) {
                    val intent = Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES).apply {
                        data = Uri.parse("package:${context.packageName}")
                        flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    }
                    context.startActivity(intent)
                    return
                }
            }

            val intent = Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(apkUri, "application/vnd.android.package-archive")
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION
            }
            context.startActivity(intent)
        } catch (e: Exception) {
            _updateState.value = UpdateState.Error("Cannot launch installer: ${e.localizedMessage}")
        }
    }

    private fun isNewerVersion(latest: String, current: String): Boolean {
        val cleanLatest = latest.removePrefix("v").substringBefore("-")
        val cleanCurrent = current.removePrefix("v").substringBefore("-")
        val latestParts = cleanLatest.split(".").mapNotNull { it.toIntOrNull() }
        val currentParts = cleanCurrent.split(".").mapNotNull { it.toIntOrNull() }
        val maxLen = maxOf(latestParts.size, currentParts.size)
        for (i in 0 until maxLen) {
            val l = latestParts.getOrElse(i) { 0 }
            val c = currentParts.getOrElse(i) { 0 }
            if (l > c) return true
            if (l < c) return false
        }
        return false
    }
}
