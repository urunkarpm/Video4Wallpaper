package com.urunkarpm.video4wallpaper.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat

@Composable
fun HomeScreen(
    viewModel: MainViewModel,
    onApplyWallpaper: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val videos by viewModel.videos.collectAsState()
    val selectedUri by viewModel.selectedUri.collectAsState()
    val loopEnabled by viewModel.loopEnabled.collectAsState()
    val pauseOnBatterySaver by viewModel.pauseOnBatterySaver.collectAsState()
    val scalingMode by viewModel.scalingMode.collectAsState()
    val soundEnabled by viewModel.soundEnabled.collectAsState()
    val playbackSpeed by viewModel.playbackSpeed.collectAsState()

    var selectedTab by remember { mutableIntStateOf(0) }

    val permissionToRequest = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        Manifest.permission.READ_MEDIA_VIDEO
    } else {
        Manifest.permission.READ_EXTERNAL_STORAGE
    }

    val permissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) {}

    LaunchedEffect(Unit) {
        if (ContextCompat.checkSelfPermission(context, permissionToRequest) != PackageManager.PERMISSION_GRANTED) {
            permissionLauncher.launch(permissionToRequest)
        }
    }

    Scaffold(
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    selected = selectedTab == 0,
                    onClick = { selectedTab = 0 },
                    label = { Text("Gallery") },
                    icon = { Icon(Icons.Default.PlayArrow, contentDescription = "Gallery") }
                )
                NavigationBarItem(
                    selected = selectedTab == 1,
                    onClick = { selectedTab = 1 },
                    label = { Text("Settings") },
                    icon = { Icon(Icons.Default.Settings, contentDescription = "Settings") }
                )
            }
        }
    ) { paddingValues ->
        Box(
            modifier = modifier
                .padding(paddingValues)
                .fillMaxSize()
        ) {
            when (selectedTab) {
                0 -> VideoPickerScreen(
                    videos = videos,
                    selectedUri = selectedUri,
                    scalingMode = scalingMode,
                    soundEnabled = soundEnabled,
                    playbackSpeed = playbackSpeed,
                    onVideoSelected = { viewModel.selectVideo(it) },
                    onImportVideo = { viewModel.importVideo(it) },
                    onRenameVideo = { video, newName -> viewModel.renameVideo(video, newName) },
                    onDeleteVideo = { viewModel.deleteVideo(it) },
                    onApplyWallpaper = {
                        if (selectedUri.isNullOrEmpty() && videos.isNotEmpty()) {
                            viewModel.selectVideo(videos.first().uri)
                        }
                        onApplyWallpaper()
                    },
                    modifier = Modifier.fillMaxSize()
                )
                1 -> SettingsScreen(
                    loopEnabled = loopEnabled,
                    onLoopChanged = { viewModel.setLoopEnabled(it) },
                    pauseOnBatterySaver = pauseOnBatterySaver,
                    onPauseOnBatterySaverChanged = { viewModel.setPauseOnBatterySaver(it) },
                    scalingMode = scalingMode,
                    onScalingModeChanged = { viewModel.setScalingMode(it) },
                    soundEnabled = soundEnabled,
                    onSoundEnabledChanged = { viewModel.setSoundEnabled(it) },
                    playbackSpeed = playbackSpeed,
                    onPlaybackSpeedChanged = { viewModel.setPlaybackSpeed(it) },
                    currentVersion = viewModel.currentVersion,
                    onCheckForUpdates = { viewModel.openGitHubReleases(context) },
                    modifier = Modifier.fillMaxSize()
                )
            }
        }
    }
}
