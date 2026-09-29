package com.urunkarpm.video4wallpaper.data.pref

import android.content.Context
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.urunkarpm.video4wallpaper.data.model.ScalingMode
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "settings")

class SettingsRepository(private val context: Context) {
    companion object {
        val SELECTED_VIDEO_URI = stringPreferencesKey("selected_video_uri")
        val LOOP_ENABLED = booleanPreferencesKey("loop_enabled")
        val PAUSE_ON_BATTERY_SAVER = booleanPreferencesKey("pause_on_battery_saver")
        val SCALING_MODE = stringPreferencesKey("scaling_mode")
        val SOUND_ENABLED = booleanPreferencesKey("sound_enabled")
        val PLAYBACK_SPEED = floatPreferencesKey("playback_speed")
    }

    val selectedVideoUri: Flow<String?> = context.dataStore.data.map { it[SELECTED_VIDEO_URI] }
    val loopEnabled: Flow<Boolean> = context.dataStore.data.map { it[LOOP_ENABLED] ?: true }
    val pauseOnBatterySaver: Flow<Boolean> = context.dataStore.data.map { it[PAUSE_ON_BATTERY_SAVER] ?: true }
    val scalingMode: Flow<ScalingMode> = context.dataStore.data.map { ScalingMode.fromName(it[SCALING_MODE]) }
    val soundEnabled: Flow<Boolean> = context.dataStore.data.map { it[SOUND_ENABLED] ?: false }
    val playbackSpeed: Flow<Float> = context.dataStore.data.map { it[PLAYBACK_SPEED] ?: 1.0f }

    suspend fun setSelectedVideoUri(uri: String) {
        context.dataStore.edit { it[SELECTED_VIDEO_URI] = uri }
    }
    suspend fun setLoopEnabled(enabled: Boolean) {
        context.dataStore.edit { it[LOOP_ENABLED] = enabled }
    }
    suspend fun setPauseOnBatterySaver(enabled: Boolean) {
        context.dataStore.edit { it[PAUSE_ON_BATTERY_SAVER] = enabled }
    }
    suspend fun setScalingMode(mode: ScalingMode) {
        context.dataStore.edit { it[SCALING_MODE] = mode.name }
    }
    suspend fun setSoundEnabled(enabled: Boolean) {
        context.dataStore.edit { it[SOUND_ENABLED] = enabled }
    }
    suspend fun setPlaybackSpeed(speed: Float) {
        context.dataStore.edit { it[PLAYBACK_SPEED] = speed }
    }
}
