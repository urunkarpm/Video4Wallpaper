# Design Spec: Production-Grade Android Video Live Wallpaper Engine

**Date:** 2026-09-27  
**Status:** Proposed / Draft  
**Target SDK:** Android 34 (Min SDK 28)  
**Language:** Kotlin 100%  
**UI:** Jetpack Compose (Material 3)  

---

## 1. Executive Summary & Goals

The goal of this project is to build an ultra-lightweight, battery-efficient Android Live Wallpaper application (`Video4Wallpaper`) that renders 1080p high-definition video wallpapers at up to 30 FPS while maintaining power consumption under 400mW (vs ~100mW for static wallpaper).

### Core Optimization Strategy
1. **Off-main Thread Surface Rendering:** Custom `HandlerThread` for rendering, bypassing UI thread completely.
2. **Hardware Decoding via Media3 ExoPlayer:** Direct Surface rendering via `MediaCodec` with zero CPU frame copies.
3. **Hardware Display State Listening:** Immediate screen-off detection via `DisplayManager` to pause playback (<100ms response).
4. **Battery-Aware & Thermal Dynamic FPS Scaling:** Adaptive framerate (30 → 20 → 15 → 10 FPS) derived from battery capacity, power-saver state, and thermal status (API 31+ `ThermalStatus`).

---

## 2. Project Architecture & Package Layout

Root Package: `com.example.videowallpaper`

```
com.example.videowallpaper/
├── wallpaper/
│   ├── VideoWallpaperService.kt      # Main Service extending WallpaperService
│   ├── VideoWallpaperEngine.kt       # Engine instance managing Surface & ExoPlayer
│   └── FrameRateController.kt        # Adaptive FPS target calculator
├── playback/
│   └── PlaybackManager.kt            # ExoPlayer lifecycle & hardware decoding controller
├── power/
│   ├── BatteryOptimizer.kt           # BatteryManager & PowerSaveMode monitor
│   ├── ThermalMonitor.kt             # ThermalService status tracking (API 31+)
│   └── PowerStateTracker.kt          # DisplayManager display state listener
├── data/
│   ├── db/
│   │   ├── AppDatabase.kt            # Room Database
│   │   ├── VideoEntity.kt            # Video metadata entity
│   │   ├── PlaybackStatsEntity.kt    # Battery & performance metrics entity
│   │   ├── VideoDao.kt               # Video CRUD queries
│   │   └── PlaybackStatsDao.kt       # Stats logging queries
│   ├── pref/
│   │   └── SettingsRepository.kt     # DataStore preferences repository
│   └── repository/
│       └── VideoRepository.kt        # Combined data repository
├── ui/
│   ├── theme/                        # Compose Material Design 3 theme
│   ├── HomeScreen.kt                 # Main wallpaper preview & activation UI
│   ├── SettingsScreen.kt             # FPS override, battery opt, loop settings
│   ├── VideoPickerScreen.kt          # Scoped storage video grid with file badges
│   ├── StatsPanel.kt                 # Real-time mW draw, FPS, and temp dashboard
│   └── MainViewModel.kt              # Central UI state holder
├── utils/
│   ├── FileManager.kt                # Content URI to local app storage helper
│   ├── MetricsCollector.kt           # Power draw (mW) & FPS calculation metrics
│   └── VideoCompressor.kt            # FFmpeg optional compression utility wrapper
└── scheduler/
    └── WallpaperSwitchWorker.kt      # WorkManager task for scheduled wallpaper changes
```

---

## 3. Data Layer Specifications

### 3.1 Room Entities

```kotlin
@Entity(tableName = "videos")
data class VideoEntity(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val uri: String,
    val fileName: String,
    val fileSizeBytes: Long,
    val durationMs: Long,
    val isCompressed: Boolean,
    val thumbnailPath: String,
    val addedTimestamp: Long = System.currentTimeMillis()
)

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

### 3.2 DataStore Preferences
- `SELECTED_VIDEO_URI` (String)
- `LOOP_ENABLED` (Boolean, default: true)
- `TARGET_FPS` (Int: 10, 15, 20, 30; default: 30)
- `BATTERY_OPTIMIZATION_ENABLED` (Boolean, default: true)
- `SCHEDULE_ENABLED` (Boolean, default: false)
- `SCHEDULE_DAY_URI` (String)
- `SCHEDULE_NIGHT_URI` (String)

---

## 4. Hardware Playback & Rendering Engine Flow

```
[DisplayManager] ---> Screen Off? ---> Pause ExoPlayer & Loop Immediately
                           | (Screen ON)
[BatteryManager] ---> Battery Level & Thermal Level ---> FrameRateController (Calculates target FPS: 10..30)
                           |
                     [HandlerThread] ---> Loop at 1000ms/targetFPS ---> ExoPlayer Hardware Surface Render
```

### Key Requirements Enforcement:
1. `ExoPlayer` configured with `C.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING`.
2. Hardware Surface bound via `player.setVideoSurface(surfaceHolder.surface)`.
3. ExoPlayer buffer limit configured to 5,000ms max buffer to cap memory usage to <150MB.

---

## 5. Battery & Power Optimization Matrix

| Condition | Target FPS | Notes |
| :--- | :--- | :--- |
| **Charging** | 30 FPS | Full quality |
| **Battery > 80%** | 30 FPS | Baseline performance |
| **Battery 50 - 80%** | 20 FPS | Slight power reduction |
| **Battery 20 - 50%** | 15 FPS | Balanced efficiency |
| **Battery < 20%** | 10 FPS | Eco mode |
| **Power Saver ON** | 10 FPS | Max battery preservation |
| **Thermal MODERATE** | 70% of current target | Dynamic throttling |
| **Thermal CRITICAL** | 40% of current target | Severe throttling |
| **Screen OFF / Hidden** | 0 FPS (Paused) | Zero rendering draw |

---

## 6. Ponytail Simplifications (YAGNI & Lightweight Architecture)

In adherence to the `/ponytail` lazy senior dev principles:
1. **Single ViewModel for Compose UI:** Rather than fragmenting into 5 different ViewModels, use a unified `MainViewModel` to keep state flow clean and minimize boilerplate.
   - *Ponytail Ceiling:* If state grows beyond 10 streams, decompose into screen-specific ViewModels.
2. **Simplified Direct File Caching:** Copy picked video URIs directly to `context.filesDir/wallpapers/` for guaranteed read access across reboots without managing long-term URI permissions.
   - *Ponytail Ceiling:* High disk usage for multiple multi-GB videos; upgrade path is scoped storage document persistable permission URI flags.
3. **Integrated Battery Drain Estimator:** Calculate estimated mW using standard Android API `BatteryManager.BATTERY_PROPERTY_CURRENT_NOW` / `CAPACITY` differentials rather than external native hardware counters.
   - *Ponytail Ceiling:* Approximate estimation on devices without battery current microamp counters; fallback uses standard linear discharge model.

---

## 7. Verification Checklist

1. **Gradle Build Verification:** `gradlew assembleDebug` passes cleanly.
2. **ADB Verification:** Connect via WiFi ADB (`192.168.1.8:36265`) to deploy and test live behavior.
3. **Screen-Off Test:** Ensure logs verify zero frame renders when device screen turns off.
4. **Adaptive FPS Test:** Force low battery intent / battery saver mode to confirm FPS drops to 10 FPS immediately.
5. **Memory Footprint:** Confirm memory stays well below 150MB target during 10-minute soak test.
