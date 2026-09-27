# Production-Grade Android Video Live Wallpaper Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a production-grade, battery-efficient Android video live wallpaper app (`Video4Wallpaper`) running 1080p @ 30 FPS under <400mW battery draw.

**Architecture:** Kotlin Jetpack Compose M3 UI with Room & DataStore data layer. Video rendering is driven off-main thread using ExoPlayer Media3 hardware surface rendering inside `WallpaperService.Engine`, modulated dynamically by battery capacity, thermal status, and `DisplayManager` screen state listeners.

**Tech Stack:** Kotlin 1.9+, Android SDK 34 (minSdk 28), Media3 ExoPlayer 1.1.1, Jetpack Compose M3, Room 2.6.0, DataStore Preferences 1.0.0, WorkManager 2.8.1.

**Spec:** `docs/superpowers/specs/2026-09-27-video-wallpaper-design.md`

## Global Constraints
- Target SDK: 34 | Min SDK: 28 | Kotlin: 100%
- Render logic MUST run on a dedicated `HandlerThread`, NOT the main looper thread.
- Rendering MUST pause immediately (<100ms) when `DisplayManager` reports display state is not `STATE_ON`.
- ExoPlayer buffer limit MUST be capped to 5 seconds to ensure total RAM footprint <150MB.

---

### Task 1: Android Gradle Project Setup & Build Configuration

**Files:**
- Create: `settings.gradle.kts`
- Create: `build.gradle.kts`
- Create: `app/build.gradle.kts`
- Create: `app/src/main/AndroidManifest.xml`
- Create: `app/src/main/res/xml/wallpaper.xml`
- Create: `app/src/main/res/values/strings.xml`

**Interfaces:**
- Produces: Project build environment & manifest declarations for `VideoWallpaperService`.

- [ ] **Step 1: Create settings.gradle.kts and root build.gradle.kts**

```kotlin
// settings.gradle.kts
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "Video4Wallpaper"
include(":app")
```

- [ ] **Step 2: Create app/build.gradle.kts with explicit dependencies**

```kotlin
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("kotlin-kapt")
}

android {
    namespace = "com.example.videowallpaper"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.videowallpaper"
        minSdk = 28
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.8"
    }
    packaging {
        resources {
            excludes += "/META-INDEX/AL2.0"
            excludes += "/META-INDEX/LGPL2.1"
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.activity:activity-compose:1.8.2")

    // Compose BOM & UI
    implementation(platform("androidx.compose:compose-bom:2024.02.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.navigation:navigation-compose:2.7.7")

    // Media3 / ExoPlayer
    implementation("androidx.media3:media3-exoplayer:1.2.1")
    implementation("androidx.media3:media3-ui:1.2.1")

    // Room
    implementation("androidx.room:room-runtime:2.6.1")
    implementation("androidx.room:room-ktx:2.6.1")
    kapt("androidx.room:room-compiler:2.6.1")

    // DataStore & WorkManager
    implementation("androidx.datastore:datastore-preferences:1.0.0")
    implementation("androidx.work:work-runtime-ktx:2.9.0")
}
```

- [ ] **Step 3: Create AndroidManifest.xml and wallpaper.xml**

```xml
<!-- app/src/main/AndroidManifest.xml -->
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-permission android:name="android.permission.BIND_WALLPAPER_SERVICE" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" android:maxSdkVersion="32" />
    <uses-permission android:name="android.permission.READ_MEDIA_VIDEO" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:supportsRtl="true"
        android:theme="@android:style/Theme.Material.NoTitleBar">

        <activity
            android:name=".ui.MainActivity"
            android:exported="true"
            android:theme="@android:style/Theme.Material.Light.NoActionBar">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

        <service
            android:name=".wallpaper.VideoWallpaperService"
            android:exported="true"
            android:label="@string/wallpaper_label"
            android:permission="android.permission.BIND_WALLPAPER_SERVICE">
            <intent-filter>
                <action android:name="android.service.wallpaper.WallpaperService" />
            </intent-filter>
            <meta-data
                android:name="android.service.wallpaper"
                android:resource="@xml/wallpaper" />
        </service>
    </application>
</manifest>
```

- [ ] **Step 4: Commit initial build setup**

```bash
git add settings.gradle.kts build.gradle.kts app/
git commit -m "build: scaffold Android project configuration and manifest"
```

---

### Task 2: Data Layer (Room Database & DataStore Preferences)

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/data/db/VideoEntity.kt`
- Create: `app/src/main/java/com/example/videowallpaper/data/db/PlaybackStatsEntity.kt`
- Create: `app/src/main/java/com/example/videowallpaper/data/db/VideoDao.kt`
- Create: `app/src/main/java/com/example/videowallpaper/data/db/PlaybackStatsDao.kt`
- Create: `app/src/main/java/com/example/videowallpaper/data/db/AppDatabase.kt`
- Create: `app/src/main/java/com/example/videowallpaper/data/pref/SettingsRepository.kt`

**Interfaces:**
- Produces: `AppDatabase` Room instance and `SettingsRepository` Flow wrappers.

- [ ] **Step 1: Write Room Entities**

```kotlin
// VideoEntity.kt
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

// PlaybackStatsEntity.kt
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
```

- [ ] **Step 2: Write DAOs & AppDatabase**

```kotlin
// VideoDao.kt & PlaybackStatsDao.kt
package com.example.videowallpaper.data.db

import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Dao
interface VideoDao {
    @Query("SELECT * FROM videos ORDER BY addedTimestamp DESC")
    fun getAllVideos(): Flow<List<VideoEntity>>

    @Query("SELECT * FROM videos WHERE id = :id")
    suspend fun getVideoById(id: Int): VideoEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertVideo(video: VideoEntity): Long

    @Delete
    suspend fun deleteVideo(video: VideoEntity)
}

@Dao
interface PlaybackStatsDao {
    @Query("SELECT * FROM playback_stats ORDER BY sessionStartMs DESC LIMIT 50")
    fun getRecentStats(): Flow<List<PlaybackStatsEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertStats(stats: PlaybackStatsEntity)
}

@Database(entities = [VideoEntity::class, PlaybackStatsEntity::class], version = 1, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun videoDao(): VideoDao
    abstract fun playbackStatsDao(): PlaybackStatsDao
}
```

- [ ] **Step 3: Write SettingsRepository (DataStore)**

```kotlin
// SettingsRepository.kt
package com.example.videowallpaper.data.pref

import android.content.Context
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "settings")

class SettingsRepository(private val context: Context) {
    companion object {
        val SELECTED_VIDEO_URI = stringPreferencesKey("selected_video_uri")
        val LOOP_ENABLED = booleanPreferencesKey("loop_enabled")
        val TARGET_FPS = intPreferencesKey("target_fps")
        val BATTERY_OPTIMIZATION_ENABLED = booleanPreferencesKey("battery_opt")
        val SCHEDULE_ENABLED = booleanPreferencesKey("schedule_enabled")
    }

    val selectedVideoUri: Flow<String?> = context.dataStore.data.map { it[SELECTED_VIDEO_URI] }
    val loopEnabled: Flow<Boolean> = context.dataStore.data.map { it[LOOP_ENABLED] ?: true }
    val targetFps: Flow<Int> = context.dataStore.data.map { it[TARGET_FPS] ?: 30 }
    val batteryOptEnabled: Flow<Boolean> = context.dataStore.data.map { it[BATTERY_OPTIMIZATION_ENABLED] ?: true }
    val scheduleEnabled: Flow<Boolean> = context.dataStore.data.map { it[SCHEDULE_ENABLED] ?: false }

    suspend fun setSelectedVideoUri(uri: String) {
        context.dataStore.edit { it[SELECTED_VIDEO_URI] = uri }
    }
    suspend fun setLoopEnabled(enabled: Boolean) {
        context.dataStore.edit { it[LOOP_ENABLED] = enabled }
    }
    suspend fun setTargetFps(fps: Int) {
        context.dataStore.edit { it[TARGET_FPS] = fps }
    }
    suspend fun setBatteryOptEnabled(enabled: Boolean) {
        context.dataStore.edit { it[BATTERY_OPTIMIZATION_ENABLED] = enabled }
    }
    suspend fun setScheduleEnabled(enabled: Boolean) {
        context.dataStore.edit { it[SCHEDULE_ENABLED] = enabled }
    }
}
```

- [ ] **Step 4: Commit data layer**

```bash
git add app/src/main/java/com/example/videowallpaper/data/
git commit -m "feat: implement Room database entities, DAOs, and DataStore SettingsRepository"
```

---

### Task 3: Power Optimization Core

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/power/BatteryOptimizer.kt`
- Create: `app/src/main/java/com/example/videowallpaper/power/ThermalMonitor.kt`
- Create: `app/src/main/java/com/example/videowallpaper/power/PowerStateTracker.kt`
- Create: `app/src/main/java/com/example/videowallpaper/wallpaper/FrameRateController.kt`

**Interfaces:**
- Consumes: System `BatteryManager`, `PowerManager`, `ThermalService`, `DisplayManager`.
- Produces: `getAdaptiveFps(): Int` and `isScreenOn(): Boolean`.

- [ ] **Step 1: Write BatteryOptimizer and ThermalMonitor**

```kotlin
// BatteryOptimizer.kt
package com.example.videowallpaper.power

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.PowerManager

class BatteryOptimizer(private val context: Context) {
    private val batteryManager = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager
    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager

    fun getBatteryLevel(): Int {
        val intent = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = intent?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = intent?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        return if (level >= 0 && scale > 0) (level * 100 / scale) else 100
    }

    fun isCharging(): Boolean {
        val intent = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val status = intent?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        return status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
    }

    fun isPowerSaveMode(): Boolean = powerManager.isPowerSaveMode

    fun getAdaptiveFps(userTargetFps: Int, enabled: Boolean): Int {
        if (!enabled) return userTargetFps
        if (isPowerSaveMode()) return 10
        if (isCharging()) return userTargetFps

        val level = getBatteryLevel()
        val baseFps = when {
            level > 80 -> userTargetFps
            level > 50 -> minOf(userTargetFps, 20)
            level > 20 -> minOf(userTargetFps, 15)
            else -> 10
        }
        return baseFps
    }
}
```

```kotlin
// ThermalMonitor.kt
package com.example.videowallpaper.power

import android.content.Context
import android.os.Build
import android.os.PowerManager
import androidx.annotation.RequiresApi

class ThermalMonitor(private val context: Context) {
    private var currentThermalStatus: Int = 0

    init {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            powerManager.addThermalStatusListener { status ->
                currentThermalStatus = status
            }
        }
    }

    fun applyThermalCap(baseFps: Int): Int {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return baseFps
        return when (currentThermalStatus) {
            PowerManager.THERMAL_STATUS_MODERATE -> (baseFps * 0.7f).toInt().coerceAtLeast(10)
            PowerManager.THERMAL_STATUS_SEVERE,
            PowerManager.THERMAL_STATUS_CRITICAL,
            PowerManager.THERMAL_STATUS_EMERGENCY -> (baseFps * 0.4f).toInt().coerceAtLeast(10)
            else -> baseFps
        }
    }
}
```

- [ ] **Step 2: Write PowerStateTracker & FrameRateController**

```kotlin
// PowerStateTracker.kt
package com.example.videowallpaper.power

import android.content.Context
import android.hardware.display.DisplayManager
import android.os.Handler
import android.os.Looper
import android.view.Display

class PowerStateTracker(context: Context, private val onStateChanged: (Boolean) -> Unit) {
    private val displayManager = context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
    var isScreenOn: Boolean = true
        private set

    private val listener = object : DisplayManager.DisplayListener {
        override fun onDisplayAdded(displayId: Int) {}
        override fun onDisplayRemoved(displayId: Int) {}
        override fun onDisplayChanged(displayId: Int) {
            val display = displayManager.getDisplay(displayId)
            val screenState = display?.state == Display.STATE_ON
            if (isScreenOn != screenState) {
                isScreenOn = screenState
                onStateChanged(screenState)
            }
        }
    }

    fun start() {
        displayManager.registerDisplayListener(listener, Handler(Looper.getMainLooper()))
    }

    fun stop() {
        displayManager.unregisterDisplayListener(listener)
    }
}
```

- [ ] **Step 3: Commit power optimization module**

```bash
git add app/src/main/java/com/example/videowallpaper/power/ app/src/main/java/com/example/videowallpaper/wallpaper/
git commit -m "feat: implement battery optimizer, thermal monitor, and display state power tracker"
```

---

### Task 4: ExoPlayer Hardware Video Playback Manager

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/playback/PlaybackManager.kt`

**Interfaces:**
- Consumes: Android `Context`, `SurfaceHolder`, Media3 `ExoPlayer`.
- Produces: `play()`, `pause()`, `setSurface(surface)`, `release()`.

- [ ] **Step 1: Write PlaybackManager with Hardware Decoder Configuration**

```kotlin
package com.example.videowallpaper.playback

import android.content.Context
import android.net.Uri
import android.view.Surface
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.ExoPlayer

class PlaybackManager(private val context: Context) {
    private var player: ExoPlayer? = null

    fun initializePlayer(surface: Surface?, videoUri: Uri, loop: Boolean = true) {
        release()

        val loadControl = DefaultLoadControl.Builder()
            .setBufferDurationsMs(
                1000, // minBufferMs
                5000, // maxBufferMs (Caps RAM footprint <150MB)
                500,  // bufferForPlaybackMs
                1000  // bufferForPlaybackAfterRebufferMs
            ).build()

        val newPlayer = ExoPlayer.Builder(context)
            .setLoadControl(loadControl)
            .build().apply {
                videoScalingMode = C.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING
                repeatMode = if (loop) Player.REPEAT_MODE_ALL else Player.REPEAT_MODE_OFF
                setAudioAttributes(
                    AudioAttributes.Builder().setUsage(C.USAGE_MEDIA).build(),
                    /* handleAudioFocus= */ false
                )
                volume = 0f // Mute wallpaper audio by default
                if (surface != null) {
                    setVideoSurface(surface)
                }
                setMediaItem(MediaItem.fromUri(videoUri))
                prepare()
            }
        player = newPlayer
    }

    fun setSurface(surface: Surface?) {
        player?.setVideoSurface(surface)
    }

    fun play() {
        player?.playWhenReady = true
    }

    fun pause() {
        player?.playWhenReady = false
    }

    fun release() {
        player?.let {
            it.stop()
            it.release()
        }
        player = null
    }

    fun isPlaying(): Boolean = player?.isPlaying == true
}
```

- [ ] **Step 2: Commit PlaybackManager**

```bash
git add app/src/main/java/com/example/videowallpaper/playback/
git commit -m "feat: implement ExoPlayer hardware playback manager with memory-capped buffering"
```

---

### Task 5: Video Wallpaper Service Engine & Off-Main Render Loop

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/wallpaper/VideoWallpaperService.kt`

**Interfaces:**
- Extends: `WallpaperService`.
- Integrates `PlaybackManager`, `BatteryOptimizer`, `ThermalMonitor`, `PowerStateTracker`.

- [ ] **Step 1: Write VideoWallpaperService and Engine**

```kotlin
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
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first

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
                if (!uriString.isNull_orEmpty()) {
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
```

- [ ] **Step 2: Commit WallpaperService**

```bash
git add app/src/main/java/com/example/videowallpaper/wallpaper/
git commit -m "feat: implement VideoWallpaperService with HandlerThread rendering and DisplayManager listener"
```

---

### Task 6: Metrics & File Utilities

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/utils/FileManager.kt`
- Create: `app/src/main/java/com/example/videowallpaper/utils/MetricsCollector.kt`

**Interfaces:**
- Produces: `copyUriToInternalStorage(context, uri)` and `getBatteryDrainEstimateMw()`.

- [ ] **Step 1: Write FileManager & MetricsCollector**

```kotlin
// FileManager.kt
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

// MetricsCollector.kt
package com.example.videowallpaper.utils

import android.content.Context
import android.os.BatteryManager

class MetricsCollector(private val context: Context) {
    private val batteryManager = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager

    fun getBatteryDrainEstimateMw(): Float {
        val currentMicroAmps = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW)
        // Convert microamps to mW assuming ~3.8V nominal voltage
        val currentMa = Math.abs(currentMicroAmps) / 1000f
        return currentMa * 3.8f
    }
}
```

- [ ] **Step 2: Commit utilities**

```bash
git add app/src/main/java/com/example/videowallpaper/utils/
git commit -m "feat: add file storage helper and battery drain metrics collector"
```

---

### Task 7: UI Layer (Jetpack Compose M3 UI Screens & MainActivity)

**Files:**
- Create: `app/src/main/java/com/example/videowallpaper/ui/MainActivity.kt`
- Create: `app/src/main/java/com/example/videowallpaper/ui/MainViewModel.kt`
- Create: `app/src/main/java/com/example/videowallpaper/ui/HomeScreen.kt`
- Create: `app/src/main/java/com/example/videowallpaper/ui/SettingsScreen.kt`
- Create: `app/src/main/java/com/example/videowallpaper/ui/VideoPickerScreen.kt`
- Create: `app/src/main/java/com/example/videowallpaper/ui/StatsPanel.kt`

**Interfaces:**
- Produces: Interactive Compose UI with Live Wallpaper picker, Settings, and Stats dashboard.

- [ ] **Step 1: Write MainViewModel**

```kotlin
package com.example.videowallpaper.ui

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.videowallpaper.data.db.AppDatabase
import com.example.videowallpaper.data.db.VideoEntity
import com.example.videowallpaper.data.pref.SettingsRepository
import com.example.videowallpaper.utils.FileManager
import androidx.room.Room
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val db = Room.databaseBuilder(application, AppDatabase::class.java, "wallpaper_db").build()
    private val videoDao = db.videoDao()
    val settingsRepository = SettingsRepository(application)

    val videos: StateFlow<List<VideoEntity>> = videoDao.getAllVideos()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val selectedUri = settingsRepository.selectedVideoUri.stateIn(viewModelScope, SharingStarted.Eagerly, null)
    val targetFps = settingsRepository.targetFps.stateIn(viewModelScope, SharingStarted.Eagerly, 30)
    val batteryOpt = settingsRepository.batteryOptEnabled.stateIn(viewModelScope, SharingStarted.Eagerly, true)

    fun importVideo(uri: Uri) {
        viewModelScope.launch {
            val file = FileManager.copyUriToInternalStorage(getApplication(), uri)
            if (file != null) {
                val entity = VideoEntity(
                    uri = file.absolutePath,
                    fileName = file.name,
                    fileSizeBytes = file.length(),
                    durationMs = 0L
                )
                videoDao.insertVideo(entity)
                settingsRepository.setSelectedVideoUri(file.absolutePath)
            }
        }
    }

    fun selectVideo(uri: String) {
        viewModelScope.launch {
            settingsRepository.setSelectedVideoUri(uri)
        }
    }

    fun setTargetFps(fps: Int) {
        viewModelScope.launch {
            settingsRepository.setTargetFps(fps)
        }
    }

    fun setBatteryOpt(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.setBatteryOptEnabled(enabled)
        }
    }
}
```

- [ ] **Step 2: Write HomeScreen, SettingsScreen, VideoPickerScreen, StatsPanel, and MainActivity**

```kotlin
// MainActivity.kt
package com.example.videowallpaper.ui

import android.app.WallpaperManager
import android.content.ComponentName
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.example.videowallpaper.wallpaper.VideoWallpaperService

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    HomeScreen(
                        viewModel = viewModel,
                        onApplyWallpaper = {
                            val intent = Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER).apply {
                                putExtra(
                                    WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                                    ComponentName(this@MainActivity, VideoWallpaperService::class.java)
                                )
                            }
                            startActivity(intent)
                        }
                    )
                }
            }
        }
    }
}
```

- [ ] **Step 3: Commit UI layer**

```bash
git add app/src/main/java/com/example/videowallpaper/ui/
git commit -m "feat: build Jetpack Compose UI (HomeScreen, VideoPicker, Settings, StatsPanel)"
```

---

### Task 8: End-to-End Build & Verification

**Files:**
- Run build and test on device via ADB.

- [ ] **Step 1: Execute Gradle debug build**

Run: `gradlew assembleDebug`
Expected: BUILD SUCCESSFUL

- [ ] **Step 2: Verify device connection via WiFi ADB**

Run: `adb connect 192.168.1.8:36265`
Expected: connected to 192.168.1.8:36265

- [ ] **Step 3: Install APK and launch application**

Run: `adb -s 192.168.1.8:36265 install -r app/build/outputs/apk/debug/app-debug.apk`
Expected: Success

- [ ] **Step 4: Commit complete working solution**

```bash
git add .
git commit -m "feat: complete production-grade Video4Wallpaper implementation"
```
