package com.example.videowallpaper.playback

import android.content.Context
import android.net.Uri
import android.view.Surface
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.ExoPlayer

class PlaybackManager(private val context: Context) {
    private var player: ExoPlayer? = null

    fun initializePlayer(surface: Surface?, videoUri: Uri, loop: Boolean = true) {
        release()

        val loadControl = DefaultLoadControl.Builder()
            .setBufferDurationsMs(
                1000, // minBufferMs
                5000, // maxBufferMs (Caps RAM footprint <150MB)
                500,  // bufferForPlaybackMs
                1000  // bufferForPlaybackAfterRebufferMs
            ).build()

        val newPlayer = ExoPlayer.Builder(context)
            .setLoadControl(loadControl)
            .build().apply {
                videoScalingMode = C.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING
                repeatMode = if (loop) Player.REPEAT_MODE_ALL else Player.REPEAT_MODE_OFF
                setAudioAttributes(
                    AudioAttributes.Builder().setUsage(C.USAGE_MEDIA).build(),
                    /* handleAudioFocus= */ false
                )
                volume = 0f // Mute wallpaper audio by default
                if (surface != null) {
                    setVideoSurface(surface)
                }
                setMediaItem(MediaItem.fromUri(videoUri))
                prepare()
            }
        player = newPlayer
    }

    fun setSurface(surface: Surface?) {
        player?.setVideoSurface(surface)
    }

    fun play() {
        player?.playWhenReady = true
    }

    fun pause() {
        player?.playWhenReady = false
    }

    fun release() {
        player?.let {
            it.stop()
            it.release()
        }
        player = null
    }

    fun isPlaying(): Boolean = player?.isPlaying == true
}
