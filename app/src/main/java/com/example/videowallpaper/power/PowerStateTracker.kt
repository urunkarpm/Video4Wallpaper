package com.example.videowallpaper.power

import android.content.Context
import android.hardware.display.DisplayManager
import android.os.Handler
import android.os.Looper
import android.view.Display

class PowerStateTracker(context: Context, private val onStateChanged: (Boolean) -> Unit) {
    private val displayManager = context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
    var isScreenOn: Boolean = true
        private set

    private val listener = object : DisplayManager.DisplayListener {
        override fun onDisplayAdded(displayId: Int) {}
        override fun onDisplayRemoved(displayId: Int) {}
        override fun onDisplayChanged(displayId: Int) {
            val display = displayManager.getDisplay(displayId)
            val screenState = display?.state == Display.STATE_ON
            if (isScreenOn != screenState) {
                isScreenOn = screenState
                onStateChanged(screenState)
            }
        }
    }

    fun start() {
        displayManager.registerDisplayListener(listener, Handler(Looper.getMainLooper()))
    }

    fun stop() {
        displayManager.unregisterDisplayListener(listener)
    }
}
