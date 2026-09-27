package com.example.videowallpaper.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun SettingsScreen(
    targetFps: Int,
    batteryOpt: Boolean,
    loopEnabled: Boolean,
    onFpsChanged: (Int) -> Unit,
    onBatteryOptChanged: (Boolean) -> Unit,
    onLoopChanged: (Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.padding(16.dp)) {
        Text(text = "Settings", style = MaterialTheme.typography.titleLarge)
        Spacer(modifier = Modifier.height(16.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = "Loop Video")
            Switch(checked = loopEnabled, onCheckedChange = onLoopChanged)
        }

        Spacer(modifier = Modifier.height(12.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = "Battery Optimization")
            Switch(checked = batteryOpt, onCheckedChange = onBatteryOptChanged)
        }

        Spacer(modifier = Modifier.height(16.dp))
        Text(text = "Target Framerate")
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            listOf(10, 15, 20, 30).forEach { fps ->
                FilterChip(
                    selected = targetFps == fps,
                    onClick = { onFpsChanged(fps) },
                    label = { Text("$fps FPS") }
                )
            }
        }
    }
}
