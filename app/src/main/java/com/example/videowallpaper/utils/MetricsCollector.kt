package com.example.videowallpaper.utils

import android.content.Context
import android.os.BatteryManager
import kotlin.math.abs

class MetricsCollector(private val context: Context) {
    private val batteryManager = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager

    fun getBatteryDrainEstimateMw(): Float {
        val currentMicroAmps = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW)
        val currentMa = abs(currentMicroAmps) / 1000f
        return currentMa * 3.8f // Nominal ~3.8V Android battery assumption
    }
}
