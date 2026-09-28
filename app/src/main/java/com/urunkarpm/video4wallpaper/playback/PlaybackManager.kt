package com.urunkarpm.video4wallpaper.playback

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.Surface
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

    fun hasPlayer(): Boolean = player != null

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
                // ponytail: Disabling audio track skips MediaCodec audio decoder initialization & AudioTrack overhead.
                // Ceiling: Wallpapers with sound won't play audio.
                // Upgrade path: Add an audio toggle setting if audible wallpapers are ever supported.
                trackSelectionParameters = trackSelectionParameters.buildUpon()
                    .setTrackTypeDisabled(C.TRACK_TYPE_AUDIO, true)
                    .build()
                if (surface != null && surface.isValid) {
                    setVideoSurface(surface)
                }
                addListener(object : Player.Listener {
                    override fun onPlayerError(error: PlaybackException) {
                        Log.e(TAG, "ExoPlayer error: ${error.message}", error)
                        // ponytail: Instant re-prepare on codec crash when surface buffers recycle.
                        // Ceiling: Repeated fatal errors loop re-preparing.
                        // Upgrade path: Add backoff counter if corrupt video file continuously faults.
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
