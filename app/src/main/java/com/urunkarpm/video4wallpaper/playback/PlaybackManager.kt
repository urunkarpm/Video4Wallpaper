package com.urunkarpm.video4wallpaper.playback

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.Surface
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.PlaybackException
import androidx.media3.common.PlaybackParameters
import androidx.media3.common.Player
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.ExoPlayer
import com.urunkarpm.video4wallpaper.data.model.ScalingMode
import java.io.File

class PlaybackManager(private val context: Context) {
    companion object {
        private const val TAG = "PlaybackManager"
    }

    private var player: ExoPlayer? = null

    fun hasPlayer(): Boolean = player != null

    fun initializePlayer(
        surface: Surface?,
        videoUri: Uri,
        loop: Boolean = true,
        autoPlay: Boolean = true,
        scalingMode: ScalingMode = ScalingMode.CROP,
        soundEnabled: Boolean = false,
        playbackSpeed: Float = 1.0f
    ) {
        release()

        val normalizedUri = if (videoUri.scheme.isNullOrEmpty()) {
            Uri.fromFile(File(videoUri.path ?: videoUri.toString()))
        } else {
            videoUri
        }

        Log.d(TAG, "Initializing ExoPlayer with URI: $normalizedUri, autoPlay=$autoPlay, scalingMode=$scalingMode, soundEnabled=$soundEnabled, speed=$playbackSpeed")

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
                videoScalingMode = scalingMode.exoScalingMode
                repeatMode = if (loop) Player.REPEAT_MODE_ALL else Player.REPEAT_MODE_OFF
                playbackParameters = PlaybackParameters(playbackSpeed)
                
                trackSelectionParameters = trackSelectionParameters.buildUpon()
                    .setTrackTypeDisabled(C.TRACK_TYPE_AUDIO, !soundEnabled)
                    .build()

                if (surface != null && surface.isValid) {
                    setVideoSurface(surface)
                }
                addListener(object : Player.Listener {
                    override fun onPlayerError(error: PlaybackException) {
                        Log.e(TAG, "ExoPlayer error: ${error.message}", error)
                        prepare()
                    }
                    override fun onPlaybackStateChanged(playbackState: Int) {
                        Log.d(TAG, "Playback state changed: $playbackState (READY=${Player.STATE_READY})")
                    }
                    override fun onRenderedFirstFrame() {
                        Log.d(TAG, "=== RENDERED FIRST FRAME ON SURFACE! ===")
                    }
                })
                setMediaItem(MediaItem.fromUri(normalizedUri))
                prepare()
            }
        player = newPlayer

        if (autoPlay) {
            play()
        }
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

    fun setScalingMode(mode: ScalingMode) {
        player?.videoScalingMode = mode.exoScalingMode
    }

    fun setSoundEnabled(enabled: Boolean) {
        player?.let { p ->
            p.trackSelectionParameters = p.trackSelectionParameters.buildUpon()
                .setTrackTypeDisabled(C.TRACK_TYPE_AUDIO, !enabled)
                .build()
        }
    }

    fun setPlaybackSpeed(speed: Float) {
        player?.playbackParameters = PlaybackParameters(speed)
    }

    fun play() {
        val p = player ?: return
        Log.d(TAG, "ExoPlayer play requested (playbackState=${p.playbackState})")
        if (p.playbackState == Player.STATE_IDLE) {
            p.prepare()
        }
        p.playWhenReady = true
    }

    fun pause() {
        Log.d(TAG, "ExoPlayer pause requested (playerIsNull=${player == null})")
        player?.playWhenReady = false
    }

    fun release() {
        Log.d(TAG, "Releasing ExoPlayer instance")
        player?.let {
            it.stop()
            it.clearVideoSurface()
            it.release()
        }
        player = null
    }

    fun isPlaying(): Boolean = player?.isPlaying == true
}
