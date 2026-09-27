package com.example.videowallpaper.utils

import android.content.Context
import android.net.Uri
import java.io.File
import java.io.FileOutputStream

object FileManager {
    fun copyUriToInternalStorage(context: Context, sourceUri: Uri): File? {
        return try {
            val wallpaperDir = File(context.filesDir, "wallpapers").apply { mkdirs() }
            val outputFile = File(wallpaperDir, "wallpaper_${System.currentTimeMillis()}.mp4")
            context.contentResolver.openInputStream(sourceUri)?.use { input ->
                FileOutputStream(outputFile).use { output ->
                    input.copyTo(output)
                }
            }
            outputFile
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }
}
