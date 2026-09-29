package com.urunkarpm.video4wallpaper.data.model

import androidx.media3.common.C

enum class ScalingMode(val exoScalingMode: Int, val label: String, val description: String) {
    CROP(
        exoScalingMode = C.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING,
        label = "Fill / Crop",
        description = "Fills entire screen while preserving aspect ratio (may crop edges)"
    ),
    FIT(
        exoScalingMode = C.VIDEO_SCALING_MODE_SCALE_TO_FIT,
        label = "Fit / Letterbox",
        description = "Fits full video frame on screen (may show top/bottom or side bars)"
    ),
    STRETCH(
        exoScalingMode = C.VIDEO_SCALING_MODE_DEFAULT,
        label = "Stretch",
        description = "Stretches video to fill screen dimensions completely"
    );

    companion object {
        fun fromName(name: String?): ScalingMode {
            return entries.find { it.name.equals(name, ignoreCase = true) } ?: CROP
        }
    }
}
