package com.example.videowallpaper.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.example.videowallpaper.utils.MetricsCollector

@Composable
fun HomeScreen(
    viewModel: MainViewModel,
    onApplyWallpaper: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val metricsCollector = remember { MetricsCollector(context) }
    val videos by viewModel.videos.collectAsState()
    val selectedUri by viewModel.selectedUri.collectAsState()
    val targetFps by viewModel.targetFps.collectAsState()
    val batteryOpt by viewModel.batteryOpt.collectAsState()
    val loopEnabled by viewModel.loopEnabled.collectAsState()

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
                    icon = {}
                )
                NavigationBarItem(
                    selected = selectedTab == 1,
                    onClick = { selectedTab = 1 },
                    label = { Text("Settings") },
                    icon = {}
                )
            }
        }
    ) { paddingValues ->
        Column(
            modifier = modifier
                .padding(paddingValues)
                .fillMaxSize()
        ) {
            ComposableStatsPanel(
                estimatedMw = metricsCollector.getBatteryDrainEstimateMw(),
                currentFps = targetFps,
                batteryOpt = batteryOpt,
                modifier = Modifier.padding(16.dp)
            )

            Button(
                onClick = {
                    if (selectedUri.isNullOrEmpty() && videos.isNotEmpty()) {
                        viewModel.selectVideo(videos.first().uri)
                    }
                    onApplyWallpaper()
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp)
            ) {
                Text("Apply as Live Wallpaper")
            }

            Spacer(modifier = Modifier.height(8.dp))

            when (selectedTab) {
                0 -> VideoPickerScreen(
                    videos = videos,
                    selectedUri = selectedUri,
                    onVideoSelected = { viewModel.selectVideo(it) },
                    onImportVideo = { viewModel.importVideo(it) },
                    modifier = Modifier.weight(1f)
                )
                1 -> SettingsScreen(
                    targetFps = targetFps,
                    batteryOpt = batteryOpt,
                    loopEnabled = loopEnabled,
                    onFpsChanged = { viewModel.setTargetFps(it) },
                    onBatteryOptChanged = { viewModel.setBatteryOpt(it) },
                    onLoopChanged = { viewModel.setLoopEnabled(it) },
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}
