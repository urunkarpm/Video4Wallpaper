package com.example.videowallpaper.data.db

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "videos")
data class VideoEntity(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val uri: String,
    val fileName: String,
    val fileSizeBytes: Long,
    val durationMs: Long,
    val isCompressed: Boolean = false,
    val thumbnailPath: String = "",
    val addedTimestamp: Long = System.currentTimeMillis()
)
