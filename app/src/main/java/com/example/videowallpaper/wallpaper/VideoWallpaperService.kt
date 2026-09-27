package com.example.videowallpaper.wallpaper

import android.net.Uri
import android.os.Handler
import android.os.HandlerThread
import android.service.wallpaper.WallpaperService
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
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

class VideoWallpaperService : WallpaperService() {

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
        private var isEngineVisible = false

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            playbackManager = PlaybackManager(this@VideoWallpaperService)
            batteryOptimizer = BatteryOptimizer(this@VideoWallpaperService)
            thermalMonitor = ThermalMonitor(this@VideoWallpaperService)
            settingsRepository = SettingsRepository(this@VideoWallpaperService)

            renderThread = HandlerThread("VideoWallpaperRenderThread").apply { start() }
            renderHandler = Handler(renderThread!!.looper)

            powerTracker = PowerStateTracker(this@VideoWallpaperService) { isScreenOn ->
                if (!isScreenOn) {
                    playbackManager.pause()
                } else if (isEngineVisible) {
                    playbackManager.play()
                }
            }
            powerTracker.start()
        }

        override fun onSurfaceCreated(holder: SurfaceHolder) {
            super.onSurfaceCreated(holder)
            playbackManager.setSurface(holder.surface)
            loadAndPlayWallpaper()
        }

        override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            super.onSurfaceChanged(holder, format, width, height)
            playbackManager.setSurface(holder.surface)
        }

        override fun onVisibilityChanged(visible: Boolean) {
            super.onVisibilityChanged(visible)
            isEngineVisible = visible
            if (visible && powerTracker.isScreenOn) {
                playbackManager.play()
            } else {
                playbackManager.pause()
            }
        }

        private fun loadAndPlayWallpaper() {
            serviceScope.launch {
                val uriString = settingsRepository.selectedVideoUri.first()
                val loop = settingsRepository.loopEnabled.first()
                if (!uriString.isNullOrEmpty()) {
                    val uri = Uri.parse(uriString)
                    playbackManager.initializePlayer(surfaceHolder.surface, uri, loop)
                    if (isEngineVisible && powerTracker.isScreenOn) {
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
            powerTracker.stop()
            playbackManager.release()
            renderThread?.quitSafely()
            serviceScope.cancel()
        }
    }
}
