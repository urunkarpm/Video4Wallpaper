package com.urunkarpm.video4wallpaper.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface VideoDao {
    @Query("SELECT * FROM videos ORDER BY addedTimestamp DESC")
    fun getAllVideos(): Flow<List<VideoEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertVideo(video: VideoEntity): Long
}
