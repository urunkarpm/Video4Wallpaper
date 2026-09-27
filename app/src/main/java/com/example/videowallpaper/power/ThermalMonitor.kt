package com.example.videowallpaper.power

import android.content.Context
import android.os.Build
import android.os.PowerManager

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
