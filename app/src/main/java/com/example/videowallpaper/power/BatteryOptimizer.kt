package com.example.videowallpaper.power

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.PowerManager

class BatteryOptimizer(private val context: Context) {
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
        return when {
            level > 80 -> userTargetFps
            level > 50 -> minOf(userTargetFps, 20)
            level > 20 -> minOf(userTargetFps, 15)
            else -> 10
        }
    }
}
