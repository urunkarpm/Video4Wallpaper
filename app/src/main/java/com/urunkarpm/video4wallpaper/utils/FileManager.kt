package com.urunkarpm.video4wallpaper.utils

import android.content.Context
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import com.urunkarpm.video4wallpaper.data.model.VideoItem
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream

object FileManager {
    private const val TAG = "FileManager"

    fun copyUriToInternalStorage(context: Context, sourceUri: Uri): File? {
        return try {
            val wallpaperDir = File(context.filesDir, "wallpapers").apply { mkdirs() }
            val timestamp = System.currentTimeMillis()
            val outputFile = File(wallpaperDir, "wallpaper_$timestamp.mp4")
            
            context.contentResolver.openInputStream(sourceUri)?.use { input ->
                FileOutputStream(outputFile).use { output ->
                    input.copyTo(output)
                }
            }

            // Extract original display name
            val originalName = getDisplayNameFromUri(context, sourceUri) ?: outputFile.name

            // Extract metadata & thumbnail
            val metadata = extractMetadataAndThumbnail(context, outputFile, originalName)
            saveVideoMetadata(outputFile, metadata)

            outputFile
        } catch (e: Exception) {
            Log.e(TAG, "Error copying URI to internal storage", e)
            null
        }
    }

    fun getDisplayNameFromUri(context: Context, uri: Uri): String? {
        var name: String? = null
        try {
            if (uri.scheme == "content") {
                context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (nameIndex != -1) {
                            name = cursor.getString(nameIndex)
                        }
                    }
                }
            }
            if (name.isNullOrEmpty()) {
                name = uri.lastPathSegment
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to resolve display name from URI", e)
        }
        return name
    }

    fun loadVideoItem(context: Context, videoFile: File): VideoItem {
        val metaFile = File(videoFile.parentFile, "${videoFile.nameWithoutExtension}.json")
        if (metaFile.exists()) {
            try {
                val json = JSONObject(metaFile.readText())
                val thumbPath = json.optString("thumbnailPath").ifEmpty { null }
                if (thumbPath != null && File(thumbPath).exists()) {
                    return VideoItem(
                        uri = videoFile.absolutePath,
                        fileName = videoFile.name,
                        displayName = json.optString("displayName", videoFile.name),
                        fileSizeBytes = videoFile.length(),
                        durationMs = json.optLong("durationMs", 0L),
                        width = json.optInt("width", 0),
                        height = json.optInt("height", 0),
                        thumbnailPath = thumbPath
                    )
                }
            } catch (e: Exception) {
                Log.w(TAG, "Error reading metadata JSON for ${videoFile.name}", e)
            }
        }

        // Generate fallback metadata & thumbnail if missing or legacy
        val item = extractMetadataAndThumbnail(context, videoFile, videoFile.name)
        saveVideoMetadata(videoFile, item)
        return item
    }

    fun updateVideoDisplayName(videoFile: File, newDisplayName: String): VideoItem {
        val current = loadVideoItemWithoutRegenerating(videoFile)
        val updated = current.copy(displayName = newDisplayName)
        saveVideoMetadata(videoFile, updated)
        return updated
    }

    private fun loadVideoItemWithoutRegenerating(videoFile: File): VideoItem {
        val metaFile = File(videoFile.parentFile, "${videoFile.nameWithoutExtension}.json")
        if (metaFile.exists()) {
            try {
                val json = JSONObject(metaFile.readText())
                return VideoItem(
                    uri = videoFile.absolutePath,
                    fileName = videoFile.name,
                    displayName = json.optString("displayName", videoFile.name),
                    fileSizeBytes = videoFile.length(),
                    durationMs = json.optLong("durationMs", 0L),
                    width = json.optInt("width", 0),
                    height = json.optInt("height", 0),
                    thumbnailPath = json.optString("thumbnailPath").ifEmpty { null }
                )
            } catch (_: Exception) {}
        }
        return VideoItem(
            uri = videoFile.absolutePath,
            fileName = videoFile.name,
            displayName = videoFile.name,
            fileSizeBytes = videoFile.length()
        )
    }

    fun saveVideoMetadata(videoFile: File, item: VideoItem) {
        try {
            val metaFile = File(videoFile.parentFile, "${videoFile.nameWithoutExtension}.json")
            val json = JSONObject().apply {
                put("displayName", item.displayName)
                put("durationMs", item.durationMs)
                put("width", item.width)
                put("height", item.height)
                put("thumbnailPath", item.thumbnailPath ?: "")
            }
            metaFile.writeText(json.toString())
        } catch (e: Exception) {
            Log.e(TAG, "Error saving video metadata for ${videoFile.name}", e)
        }
    }

    fun deleteVideoAndMetadata(videoFile: File) {
        try {
            val item = loadVideoItemWithoutRegenerating(videoFile)
            if (videoFile.exists()) videoFile.delete()
            val metaFile = File(videoFile.parentFile, "${videoFile.nameWithoutExtension}.json")
            if (metaFile.exists()) metaFile.delete()
            item.thumbnailPath?.let { path ->
                val thumb = File(path)
                if (thumb.exists()) thumb.delete()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting video and associated files", e)
        }
    }

    private fun extractMetadataAndThumbnail(context: Context, videoFile: File, displayName: String): VideoItem {
        var durationMs = 0L
        var width = 0
        var height = 0
        var thumbnailPath: String? = null

        val retriever = MediaMetadataRetriever()
        try {
            retriever.setDataSource(videoFile.absolutePath)
            val durationStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)
            durationMs = durationStr?.toLongOrNull() ?: 0L

            val widthStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)
            width = widthStr?.toIntOrNull() ?: 0

            val heightStr = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)
            height = heightStr?.toIntOrNull() ?: 0

            // Capture frame at 1s (1,000,000 microseconds)
            val bitmap = retriever.getFrameAtTime(1_000_000, MediaMetadataRetriever.OPTION_CLOSEST_SYNC)
                ?: retriever.frameAtTime

            if (bitmap != null) {
                val thumbDir = File(context.filesDir, "thumbnails").apply { mkdirs() }
                val thumbFile = File(thumbDir, "${videoFile.nameWithoutExtension}_thumb.jpg")
                FileOutputStream(thumbFile).use { out ->
                    bitmap.compress(Bitmap.CompressFormat.JPEG, 85, out)
                }
                thumbnailPath = thumbFile.absolutePath
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed extracting metadata/thumbnail for ${videoFile.name}", e)
        } finally {
            runCatching { retriever.release() }
        }

        return VideoItem(
            uri = videoFile.absolutePath,
            fileName = videoFile.name,
            displayName = displayName,
            fileSizeBytes = videoFile.length(),
            durationMs = durationMs,
            width = width,
            height = height,
            thumbnailPath = thumbnailPath
        )
    }
}
