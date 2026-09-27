package com.example.videowallpaper.data.db

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "playback_stats")
data class PlaybackStatsEntity(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val videoId: Int,
    val sessionStartMs: Long,
    val sessionDurationMs: Long,
    val avgBatteryDrainMw: Float,
    val peakTempC: Float,
    val avgFps: Float,
    val frameDropCount: Int
)
