package com.example.videowallpaper.wallpaper

import android.graphics.PixelFormat
import android.net.Uri
import android.os.Handler
import android.os.HandlerThread
import android.service.wallpaper.WallpaperService
import android.util.Log
import android.view.SurfaceHolder
import com.example.videowallpaper.data.pref.SettingsRepository
import com.example.videowallpaper.playback.PlaybackManager
import com.example.videowallpaper.power.BatteryOptimizer
import com.example.videowallpaper.power.PowerStateTracker
import com.example.videowallpaper.power.ThermalMonitor
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
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
        private lateinit var batteryOptimizer: BatteryOptimizer
        private lateinit var thermalMonitor: ThermalMonitor
        private lateinit var powerTracker: PowerStateTracker
        private lateinit var settingsRepository: SettingsRepository

        private var renderThread: HandlerThread? = null
        private var renderHandler: Handler? = null
        private var isEngineVisible = true // Default visible for preview/active

        private var currentVideoUri: Uri? = null
        private var isLoopEnabled: Boolean = true

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            Log.d(TAG, "onCreate called")
            playbackManager = PlaybackManager(this@VideoWallpaperService)
            batteryOptimizer = BatteryOptimizer(this@VideoWallpaperService)
            thermalMonitor = ThermalMonitor(this@VideoWallpaperService)
            settingsRepository = SettingsRepository(this@VideoWallpaperService)

            renderThread = HandlerThread("VideoWallpaperRenderThread").apply { start() }
            renderHandler = Handler(renderThread!!.looper)

            powerTracker = PowerStateTracker(this@VideoWallpaperService) { isScreenOn ->
                Log.d(TAG, "Screen state changed: isScreenOn=$isScreenOn, isEngineVisible=$isEngineVisible")
                if (!isScreenOn) {
                    playbackManager.pause()
                } else if (isEngineVisible) {
                    playbackManager.play()
                }
            }
            powerTracker.start()
            observeSettings()
        }

        override fun onSurfaceCreated(holder: SurfaceHolder) {
            super.onSurfaceCreated(holder)
            Log.d(TAG, "onSurfaceCreated called")
            holder.setFormat(PixelFormat.RGBA_8888)
            playbackManager.setSurface(holder.surface)
            updatePlayer()
        }

        override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            super.onSurfaceChanged(holder, format, width, height)
            Log.d(TAG, "onSurfaceChanged called (${width}x${height})")
            holder.setFormat(PixelFormat.RGBA_8888)
            playbackManager.setSurface(holder.surface)
            updatePlayer()
        }

        override fun onVisibilityChanged(visible: Boolean) {
            super.onVisibilityChanged(visible)
            Log.d(TAG, "onVisibilityChanged: visible=$visible, isScreenOn=${powerTracker.isScreenOn}")
            isEngineVisible = visible
            if (visible && powerTracker.isScreenOn) {
                playbackManager.play()
            } else {
                playbackManager.pause()
            }
        }

        private fun observeSettings() {
            serviceScope.launch {
                settingsRepository.selectedVideoUri.collect { uriString ->
                    Log.d(TAG, "Observed selectedVideoUri change: $uriString")
                    if (!uriString.isNullOrEmpty()) {
                        currentVideoUri = if (uriString.startsWith("/")) {
                            Uri.fromFile(File(uriString))
                        } else {
                            Uri.parse(uriString)
                        }
                        updatePlayer()
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
                val shouldPlay = isEngineVisible && powerTracker.isScreenOn
                Log.d(TAG, "updatePlayer: initializing ExoPlayer with URI=$uri, shouldPlay=$shouldPlay")
                playbackManager.initializePlayer(surface, uri, isLoopEnabled, autoPlay = shouldPlay)
            } else {
                Log.d(TAG, "updatePlayer: surface not valid yet")
            }
        }

        override fun onSurfaceDestroyed(holder: SurfaceHolder) {
            super.onSurfaceDestroyed(holder)
            Log.d(TAG, "onSurfaceDestroyed called")
            playbackManager.pause()
            playbackManager.setSurface(null)
        }

        override fun onDestroy() {
            super.onDestroy()
            Log.d(TAG, "onDestroy called")
            powerTracker.stop()
            playbackManager.release()
            renderThread?.quitSafely()
            serviceScope.cancel()
        }
    }
}
