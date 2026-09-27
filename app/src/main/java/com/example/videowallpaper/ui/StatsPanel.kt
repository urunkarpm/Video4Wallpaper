package com.example.videowallpaper.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
ComposableStatsPanel(
    estimatedMw: Float,
    currentFps: Int,
    batteryOpt: Boolean,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "Performance Dashboard",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Spacer(modifier = Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(text = "Power Draw: ${"%.1f".format(estimatedMw)} mW")
                Text(text = "Target FPS: $currentFps")
            }
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = if (batteryOpt) "Battery Optimization: ACTIVE" else "Battery Optimization: DISABLED",
                style = MaterialTheme.typography.bodySmall,
                color = if (batteryOpt) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
            )
        }
    }
}
