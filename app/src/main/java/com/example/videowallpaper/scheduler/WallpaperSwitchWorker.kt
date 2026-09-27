package com.example.videowallpaper.scheduler

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.example.videowallpaper.data.pref.SettingsRepository
import kotlinx.coroutines.flow.first

class WallpaperSwitchWorker(
    context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        val settings = SettingsRepository(applicationContext)
        val scheduleEnabled = settings.scheduleEnabled.first()
        if (!scheduleEnabled) return Result.success()

        val nextUri = inputData.getString("target_video_uri") ?: return Result.failure()
        settings.setSelectedVideoUri(nextUri)
        return Result.success()
    }
}
