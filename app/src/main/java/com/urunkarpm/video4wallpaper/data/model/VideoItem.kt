package com.urunkarpm.video4wallpaper.data.model

data class VideoItem(
    val uri: String,
    val fileName: String,
    val displayName: String = fileName,
    val fileSizeBytes: Long = 0L,
    val durationMs: Long = 0L,
    val width: Int = 0,
    val height: Int = 0,
    val thumbnailPath: String? = null
)
