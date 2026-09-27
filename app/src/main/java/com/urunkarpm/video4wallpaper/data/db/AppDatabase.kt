package com.urunkarpm.video4wallpaper.data.db

import androidx.room.Database
import androidx.room.RoomDatabase

@Database(entities = [VideoEntity::class], version = 1, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun videoDao(): VideoDao
}
