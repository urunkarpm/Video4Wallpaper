package com.example.videowallpaper.wallpaper

import android.content.Context
import com.example.videowallpaper.power.BatteryOptimizer
import com.example.videowallpaper.power.ThermalMonitor

class FrameRateController(context: Context) {
    private val batteryOptimizer = BatteryOptimizer(context)
    private val thermalMonitor = ThermalMonitor(context)

    fun calculateTargetFps(userTargetFps: Int, isBatteryOptEnabled: Boolean): Int {
        val adaptiveFps = batteryOptimizer.getAdaptiveFps(userTargetFps, isBatteryOptEnabled)
        return thermalMonitor.applyThermalCap(adaptiveFps)
    }
}
