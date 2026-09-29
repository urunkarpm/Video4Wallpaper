package com.urunkarpm.video4wallpaper.ui

import android.app.Application
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.urunkarpm.video4wallpaper.data.model.ScalingMode
import com.urunkarpm.video4wallpaper.data.model.VideoItem
import com.urunkarpm.video4wallpaper.data.pref.SettingsRepository
import com.urunkarpm.video4wallpaper.utils.FileManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import java.io.File

class MainViewModel(application: Application) : AndroidViewModel(application) {
    val settingsRepository = SettingsRepository(application)
    private val wallpaperDir = File(application.filesDir, "wallpapers").apply { mkdirs() }

    val currentVersion: String = runCatching {
        application.packageManager.getPackageInfo(application.packageName, 0).versionName
    }.getOrNull() ?: "1.2.0"

    private val _videos = MutableStateFlow<List<VideoItem>>(emptyList())
    val videos: StateFlow<List<VideoItem>> = _videos.asStateFlow()

    val selectedUri: StateFlow<String?> = settingsRepository.selectedVideoUri
        .stateIn(viewModelScope, SharingStarted.Eagerly, null)

    val loopEnabled: StateFlow<Boolean> = settingsRepository.loopEnabled
        .stateIn(viewModelScope, SharingStarted.Eagerly, true)

    val pauseOnBatterySaver: StateFlow<Boolean> = settingsRepository.pauseOnBatterySaver
        .stateIn(viewModelScope, SharingStarted.Eagerly, true)

    val scalingMode: StateFlow<ScalingMode> = settingsRepository.scalingMode
        .stateIn(viewModelScope, SharingStarted.Eagerly, ScalingMode.CROP)

    val soundEnabled: StateFlow<Boolean> = settingsRepository.soundEnabled
        .stateIn(viewModelScope, SharingStarted.Eagerly, false)

    val playbackSpeed: StateFlow<Float> = settingsRepository.playbackSpeed
        .stateIn(viewModelScope, SharingStarted.Eagerly, 1.0f)

    init {
        loadVideos()
    }

    fun loadVideos() {
        viewModelScope.launch(Dispatchers.IO) {
            val files = wallpaperDir.listFiles()
                ?.filter { it.isFile && it.extension.lowercase() in listOf("mp4", "webm", "gif", "mkv", "3gp") }
                ?.sortedByDescending { it.lastModified() }
                ?.map { file ->
                    FileManager.loadVideoItem(getApplication(), file)
                } ?: emptyList()
            _videos.value = files
        }
    }

    fun importVideo(uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            val file = FileManager.copyUriToInternalStorage(getApplication(), uri)
            if (file != null) {
                loadVideos()
                settingsRepository.setSelectedVideoUri(file.absolutePath)
            }
        }
    }

    fun selectVideo(uri: String) {
        viewModelScope.launch {
            settingsRepository.setSelectedVideoUri(uri)
        }
    }

    fun renameVideo(video: VideoItem, newDisplayName: String) {
        viewModelScope.launch(Dispatchers.IO) {
            val file = File(video.uri)
            if (file.exists()) {
                FileManager.updateVideoDisplayName(file, newDisplayName)
                loadVideos()
            }
        }
    }

    fun deleteVideo(video: VideoItem) {
        viewModelScope.launch(Dispatchers.IO) {
            val file = File(video.uri)
            FileManager.deleteVideoAndMetadata(file)
            loadVideos()
            if (selectedUri.value == video.uri) {
                val remaining = _videos.value
                settingsRepository.setSelectedVideoUri(remaining.firstOrNull()?.uri ?: "")
            }
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

    fun setScalingMode(mode: ScalingMode) {
        viewModelScope.launch {
            settingsRepository.setScalingMode(mode)
        }
    }

    fun setSoundEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.setSoundEnabled(enabled)
        }
    }

    fun setPlaybackSpeed(speed: Float) {
        viewModelScope.launch {
            settingsRepository.setPlaybackSpeed(speed)
        }
    }

    fun openGitHubReleases(context: Context) {
        try {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/urunkarpm/Video4Wallpaper/releases")).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
            context.startActivity(intent)
        } catch (_: Exception) {}
    }
}
