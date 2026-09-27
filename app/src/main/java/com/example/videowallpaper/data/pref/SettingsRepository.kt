package com.example.videowallpaper.data.pref

import android.content.Context
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
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
