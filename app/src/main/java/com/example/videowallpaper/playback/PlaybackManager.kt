package com.example.videowallpaper.playback

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.Surface
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.ExoPlayer
import java.io.File

class PlaybackManager(private val context: Context) {
    companion object {
        private const val TAG = "PlaybackManager"
    }

    private var player: ExoPlayer? = null

    fun initializePlayer(surface: Surface?, videoUri: Uri, loop: Boolean = true, autoPlay: Boolean = true) {
        release()

        val normalizedUri = if (videoUri.scheme.isNullOrEmpty()) {
            Uri.fromFile(File(videoUri.path ?: videoUri.toString()))
        } else {
            videoUri
        }

        Log.d(TAG, "Initializing ExoPlayer with URI: $normalizedUri, autoPlay=$autoPlay")

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
                if (surface != null && surface.isValid) {
                    setVideoSurface(surface)
                }
                addListener(object : Player.Listener {
                    override fun onPlayerError(error: PlaybackException) {
                        Log.e(TAG, "ExoPlayer error: ${error.message}", error)
                    }
                    override fun onPlaybackStateChanged(playbackState: Int) {
                        Log.d(TAG, "Playback state changed: $playbackState (READY=${Player.STATE_READY})")
                    }
                    override fun onRenderedFirstFrame() {
                        Log.d(TAG, "=== RENDERED FIRST FRAME ON SURFACE! ===")
                    }
                })
                setMediaItem(MediaItem.fromUri(normalizedUri))
                playWhenReady = autoPlay
                prepare()
            }
        player = newPlayer
    }

    fun setSurface(surface: Surface?) {
        if (surface != null && surface.isValid) {
            Log.d(TAG, "Setting valid surface on ExoPlayer")
            player?.setVideoSurface(surface)
        } else {
            Log.d(TAG, "Clearing video surface on ExoPlayer")
            player?.clearVideoSurface()
        }
    }

    fun play() {
        Log.d(TAG, "ExoPlayer play requested (playerIsNull=${player == null})")
        player?.playWhenReady = true
    }

    fun pause() {
        Log.d(TAG, "ExoPlayer pause requested (playerIsNull=${player == null})")
        player?.playWhenReady = false
    }

    fun release() {
        Log.d(TAG, "Releasing ExoPlayer instance")
        player?.let {
            it.stop()
            it.release()
        }
        player = null
    }

    fun isPlaying(): Boolean = player?.isPlaying == true
}
