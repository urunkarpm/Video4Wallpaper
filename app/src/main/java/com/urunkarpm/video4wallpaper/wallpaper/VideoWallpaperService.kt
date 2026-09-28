package com.urunkarpm.video4wallpaper.wallpaper

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.PowerManager
import android.service.wallpaper.WallpaperService
import android.util.Log
import android.view.SurfaceHolder
import androidx.core.content.ContextCompat
import com.urunkarpm.video4wallpaper.data.pref.SettingsRepository
import com.urunkarpm.video4wallpaper.playback.PlaybackManager
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

        private var currentVideoUri: Uri? = null
        private var isLoopEnabled: Boolean = true
        private var pauseOnBatterySaver: Boolean = true

        private val screenAndPowerReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                Log.d(TAG, "screenAndPowerReceiver: action=${intent?.action}, isScreenOn=${isScreenOn()}, isPowerSave=${isPowerSaveMode()}")
                updatePlaybackState()
            }
        }

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            playbackManager = PlaybackManager(this@VideoWallpaperService)
            settingsRepository = SettingsRepository(this@VideoWallpaperService)
            val filter = IntentFilter().apply {
                addAction(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED)
                addAction(Intent.ACTION_SCREEN_OFF)
                addAction(Intent.ACTION_SCREEN_ON)
            }
            ContextCompat.registerReceiver(
                this@VideoWallpaperService,
                screenAndPowerReceiver,
                filter,
                ContextCompat.RECEIVER_EXPORTED
            )
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

        override fun onVisibilityChanged(visible: Boolean) {
            super.onVisibilityChanged(visible)
            Log.d(TAG, "onVisibilityChanged: visible=$visible, isPreview=$isPreview")
            // Ignored intentionally: Android triggers visible=false when opening status bar, notification shade, or app drawer.
            // Continuous smooth playback is maintained; pauses only when screen turns off or battery saver is active.
        }

        private fun isScreenOn(): Boolean {
            val pm = getSystemService(Context.POWER_SERVICE) as? PowerManager
            return pm?.isInteractive == true
        }

        private fun isPowerSaveMode(): Boolean {
            val pm = getSystemService(Context.POWER_SERVICE) as? PowerManager
            return pm?.isPowerSaveMode == true
        }

        private fun shouldPlay(): Boolean {
            if (!isScreenOn()) return false
            if (pauseOnBatterySaver && isPowerSaveMode()) return false
            return true
        }

        // ponytail: Keeps video playing continuously while screen is interactive (screen on) to eliminate status bar / app drawer stutter and 1-sec wake latency.
        // Ceiling: Continues decoding video even when an opaque full-screen app covers the home screen until screen turns off or surface is destroyed.
        // Upgrade path: If background app battery drain is an issue, listen to window insets or delayed visibility debouncer (e.g. 5s grace period before pausing).
        private fun updatePlaybackState() {
            if (shouldPlay()) {
                val surface = surfaceHolder?.surface
                if (surface != null && surface.isValid) {
                    playbackManager.setSurface(surface)
                }
                playbackManager.play()
            } else {
                playbackManager.pause()
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
            serviceScope.launch {
                settingsRepository.pauseOnBatterySaver.collect { pause ->
                    pauseOnBatterySaver = pause
                    updatePlaybackState()
                }
            }
        }

        private fun updatePlayer() {
            val uri = currentVideoUri ?: return
            val surface = surfaceHolder?.surface
            if (surface != null && surface.isValid) {
                if (!playbackManager.hasPlayer()) {
                    Log.d(TAG, "updatePlayer: initializing ExoPlayer with URI=$uri, isPreview=$isPreview")
                    playbackManager.initializePlayer(surface, uri, isLoopEnabled, autoPlay = shouldPlay())
                } else {
                    playbackManager.setSurface(surface)
                    if (shouldPlay()) {
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
            runCatching { this@VideoWallpaperService.unregisterReceiver(screenAndPowerReceiver) }
            playbackManager.release()
            serviceScope.cancel()
        }
    }
}
