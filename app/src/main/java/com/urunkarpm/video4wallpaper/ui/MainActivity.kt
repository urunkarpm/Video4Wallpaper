package com.urunkarpm.video4wallpaper.ui

import android.app.WallpaperManager
import android.content.ComponentName
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import com.urunkarpm.video4wallpaper.wallpaper.VideoWallpaperService

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        // ponytail: enableEdgeToEdge is the standard 1-liner to eliminate system bar shadow/scrim and unify with the app surface.
        // Ceiling: Uses SystemBarStyle.auto; upgrade path: pass custom styles if individual screens need distinct status bar palettes.
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            // ponytail: Native M3 dynamic colors with dark mode detection adapts to system palette with zero boilerplate.
            // Ceiling: Fixed to system palette; upgrade path: add manual light/dark override toggle in Settings if requested.
            val darkTheme = isSystemInDarkTheme()
            val context = LocalContext.current
            val colorScheme = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (darkTheme) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
            } else {
                if (darkTheme) darkColorScheme() else lightColorScheme()
            }

            MaterialTheme(colorScheme = colorScheme) {
                Surface(modifier = Modifier.fillMaxSize()) {
                    HomeScreen(
                        viewModel = viewModel,
                        onApplyWallpaper = {
                            try {
                                val intent = Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER).apply {
                                    putExtra(
                                        WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                                        ComponentName(this@MainActivity, VideoWallpaperService::class.java)
                                    )
                                }
                                startActivity(intent)
                            } catch (e: Exception) {
                                try {
                                    val fallbackIntent = Intent(WallpaperManager.ACTION_LIVE_WALLPAPER_CHOOSER)
                                    startActivity(fallbackIntent)
                                } catch (e2: Exception) {
                                    Toast.makeText(this@MainActivity, "Could not open Live Wallpaper chooser", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                    )
                }
            }
        }
    }
}
