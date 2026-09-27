package com.example.videowallpaper.ui

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.room.Room
import com.example.videowallpaper.data.db.AppDatabase
import com.example.videowallpaper.data.db.VideoEntity
import com.example.videowallpaper.data.pref.SettingsRepository
import com.example.videowallpaper.utils.FileManager
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val db = Room.databaseBuilder(application, AppDatabase::class.java, "wallpaper_db").build()
    private val videoDao = db.videoDao()
    val settingsRepository = SettingsRepository(application)

    val videos: StateFlow<List<VideoEntity>> = videoDao.getAllVideos()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val selectedUri: StateFlow<String?> = settingsRepository.selectedVideoUri
        .stateIn(viewModelScope, SharingStarted.Eagerly, null)

    val loopEnabled: StateFlow<Boolean> = settingsRepository.loopEnabled
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
}
