package com.example.videowallpaper.wallpaper

import android.net.Uri
import android.service.wallpaper.WallpaperService
import android.util.Log
import android.view.SurfaceHolder
import com.example.videowallpaper.data.pref.SettingsRepository
import com.example.videowallpaper.playback.PlaybackManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.io.File

class VideoWallpaperService : WallpaperService() {

    companion object {
        private const val TAG = "VideoWallpaperService"
    }

    override fun onCreateEngine(): Engine = VideoEngine()

    inner class VideoEngine : Engine() {
        private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
        private lateinit var playbackManager: PlaybackManager
        private lateinit var settingsRepository: SettingsRepository

        private var isEngineVisible = false
        private var currentVideoUri: Uri? = null
        private var isLoopEnabled: Boolean = true

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            playbackManager = PlaybackManager(this@VideoWallpaperService)
            settingsRepository = SettingsRepository(this@VideoWallpaperService)
            observeSettings()
        }

        override fun onSurfaceCreated(holder: SurfaceHolder) {
            super.onSurfaceCreated(holder)
            playbackManager.setSurface(holder.surface)
            updatePlayer()
        }

        override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            super.onSurfaceChanged(holder, format, width, height)
            playbackManager.setSurface(holder.surface)
            updatePlayer()
        }

        // ponytail: Native WallpaperService.Engine.onVisibilityChanged handles screen on/off & foreground app switches.
        // Ceiling: Doesn't track fine-grained thermal or battery throttling directly inside wallpaper service.
        // Upgrade path: Attach thermal/battery listeners if device overheating throttling is needed.
        override fun onVisibilityChanged(visible: Boolean) {
            super.onVisibilityChanged(visible)
            Log.d(TAG, "onVisibilityChanged: visible=$visible, isPreview=$isPreview")
            isEngineVisible = visible
            if (visible) {
                val surface = surfaceHolder?.surface
                if (surface != null && surface.isValid) {
                    playbackManager.setSurface(surface)
                }
                playbackManager.play()
            } else {
                playbackManager.pause()
                playbackManager.setSurface(null)
            }
        }

        private fun observeSettings() {
            serviceScope.launch {
                settingsRepository.selectedVideoUri.collect { uriString ->
                    if (!uriString.isNullOrEmpty()) {
                        val newUri = if (uriString.startsWith("/")) {
                            Uri.fromFile(File(uriString))
                        } else {
                            Uri.parse(uriString)
                        }
                        if (newUri != currentVideoUri) {
                            currentVideoUri = newUri
                            playbackManager.release()
                            updatePlayer()
                        }
                    }
                }
            }
            serviceScope.launch {
                settingsRepository.loopEnabled.collect { loop ->
                    isLoopEnabled = loop
                }
            }
        }

        private fun updatePlayer() {
            val uri = currentVideoUri ?: return
            val surface = surfaceHolder?.surface
            if (surface != null && surface.isValid) {
                if (!playbackManager.hasPlayer()) {
                    Log.d(TAG, "updatePlayer: initializing ExoPlayer with URI=$uri, isPreview=$isPreview")
                    playbackManager.initializePlayer(surface, uri, isLoopEnabled, autoPlay = isEngineVisible || isPreview)
                } else {
                    playbackManager.setSurface(surface)
                    if (isEngineVisible || isPreview) {
                        playbackManager.play()
                    }
                }
            }
        }

        override fun onSurfaceDestroyed(holder: SurfaceHolder) {
            super.onSurfaceDestroyed(holder)
            playbackManager.pause()
            playbackManager.setSurface(null)
        }

        override fun onDestroy() {
            super.onDestroy()
            playbackManager.release()
            serviceScope.cancel()
        }
    }
}
