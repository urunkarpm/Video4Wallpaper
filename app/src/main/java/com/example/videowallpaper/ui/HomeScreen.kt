package com.example.videowallpaper.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
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
                onClick = onApplyWallpaper,
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
