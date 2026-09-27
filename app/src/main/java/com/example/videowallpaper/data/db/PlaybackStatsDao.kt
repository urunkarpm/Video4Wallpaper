package com.example.videowallpaper.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface PlaybackStatsDao {
    @Query("SELECT * FROM playback_stats ORDER BY sessionStartMs DESC LIMIT 50")
    fun getRecentStats(): Flow<List<PlaybackStatsEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertStats(stats: PlaybackStatsEntity)
}
