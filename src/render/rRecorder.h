/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2026 The Armagetron Advanced Development Team

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

***************************************************************************

*/

#ifndef ArmageTron_RECORDER_H
#define ArmageTron_RECORDER_H

#include "tString.h"

//! A small built-in video recorder. Two modes share one frame-capture path:
//!
//!  - Take recording (one key toggles an MP4, written to a folder the player can
//!    set). Frames go straight to disk as they are produced.
//!  - Instant replay (a rolling buffer of the last few seconds, already encoded
//!    and kept in memory). One key muxes the last MOVIE_CLIP_SECONDS into an MP4
//!    without re-encoding. This is the Medal-style clipper.
//!
//! Both use the platform's hardware encoder. MOVIE_QUALITY sets the bitrate,
//! MOVIE_CLIP_SECONDS the clip length; the buffer's length and memory budget
//! follow from those and MOVIE_BUFFER_MAX_MB.
namespace rRecorder
{
    //! starts a take if idle, otherwise stops and finalises it
    void Toggle();

    //! stops and finalises any take or buffer in progress; safe to call at any
    //! time, and meant to run during shutdown while the GL context still exists
    void Shutdown();

    //! muxes the last seconds of the buffer into an MP4. seconds <= 0 uses
    //! MOVIE_CLIP_SECONDS. Does nothing if the buffer is not running.
    void SaveClip( double seconds = 0 );

    //! called by the platform backend when a background clip save finishes
    void OnClipSaved( bool ok, tString const & path, long long bytes );

    //! called once per presented frame with the GL context current. It is cheap
    //! when idle, and it throttles to MOVIE_FPS when a take or buffer is running.
    void OnFrame( int width, int height );

    extern tString sr_movieDir;        //!< MOVIE_DIR: folder recordings are written to
    extern int     sr_movieFPS;        //!< MOVIE_FPS: capture rate, independent of render rate
    extern bool    sr_movieAudio;      //!< MOVIE_AUDIO: put the game's sound in the file
    extern int     sr_movieMaxHeight;  //!< MOVIE_MAX_HEIGHT: tallest encoded video; 0 = native
    extern int     sr_movieCodec;      //!< MOVIE_CODEC: 0 H.264, 1 HEVC
    extern int     sr_movieQuality;    //!< MOVIE_QUALITY: 0 low, 1 medium, 2 high, 3 ultra

    extern bool    sr_movieBuffer;      //!< MOVIE_BUFFER: keep the rolling instant-replay buffer
    extern int     sr_movieClipSeconds; //!< MOVIE_CLIP_SECONDS: length of a saved clip
    extern int     sr_movieBufferMaxMB; //!< MOVIE_BUFFER_MAX_MB: memory budget for the buffer

    // Derived values. Quality sets the bitrate; the buffer's length and budget
    // follow from the clip length and the memory setting.

    //! bits per pixel per frame, in thousandths, for the current quality setting.
    //! Internal: MovieVideoBitrateBps() is what callers want.
    static int MovieBitsPerPixelMilli();
    //! keyframe interval in seconds
    int MovieKeyframeSeconds();
    //! bitrate (bps) the audio track is encoded at
    int MovieAudioBitrateBps();
    //! average video bitrate (bps) the chosen quality asks for
    int MovieVideoBitrateBps();
    //! how much gameplay the rolling buffer keeps (seconds)
    int MovieBufferSeconds();
    //! memory cap for the encoded ring (bytes)
    long long MovieBufferMaxBytes();

    //! the mixer's output format, reported by the sound system once audio is up.
    //! A frequency of 0 means "no audio available".
    void SetAudioFormat( int frequency, int channels );

    //! true when a take or buffer is running whose audio belongs in the file
    bool AudioWanted();

    //! called from the audio callback with the finished mix. Only writes into a
    //! lock-free ring, so it never blocks audio.
    void OnAudioSamples( const unsigned char * data, int bytes );
}

// Platform layer, implemented by rRecorderMac.mm (macOS) or rRecorderStub.cpp.
// The stub returns false everywhere, so the feature is simply absent.
// audioFrequency 0 means "no audio track".
bool sr_RecorderPlatformBegin( tString const & path, int width, int height,
                               int audioFrequency, int audioChannels );
bool sr_RecorderPlatformFrame( double seconds );

//! false once the encoder has failed and the take can no longer be written
bool sr_RecorderPlatformIsHealthy();

// Replay buffer: starts a rolling in-memory encode with no output file yet.
bool sr_RecorderPlatformBufferBegin( int width, int height,
                                     int audioFrequency, int audioChannels );
//! muxes the last seconds of buffered samples into path; returns false on failure
bool sr_RecorderPlatformClip( double seconds, tString const & path );
//! stops the buffer and frees the encoder and ring
void sr_RecorderPlatformBufferEnd();

//! hands mixed audio to the encoder; returns false if it is behind and the data
//! should be offered again later
bool sr_RecorderPlatformAudio( const unsigned char * data, int bytes, double seconds );

//! true when the current mode has an audio track
bool sr_RecorderPlatformHasAudio();

//! finishes the take; returns the recorded length in seconds (0 if unknown)
double sr_RecorderPlatformEnd();

#endif
