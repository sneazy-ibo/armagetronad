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

#include "rRecorder.h"

#include "rConsole.h"
#include "tConfiguration.h"
#include "tSysTime.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

#ifdef WIN32
#include <direct.h>
#define sr_mkdir( p ) _mkdir( p )
#else
#include <sys/stat.h>
#include <sys/types.h>
#define sr_mkdir( p ) mkdir( p, 0755 )
#endif

namespace rRecorder
{
    //! where recordings are written; created on first use
    tString sr_movieDir( "~/Movies/Armagetron" );
    static tConfItem<tString> sr_movieDirConf( "MOVIE_DIR", sr_movieDir );

    //! capture rate. The game renders far faster than this; extra frames are skipped.
    int sr_movieFPS = 60;
    static tConfItem<int> sr_movieFPSConf( "MOVIE_FPS", sr_movieFPS );

    //! put the game's sound in the recording. The mixer keeps running even when
    //! the speakers are muted, so muting never costs a recording its audio.
    bool sr_movieAudio = true;
    static tConfItem<bool> sr_movieAudioConf( "MOVIE_AUDIO", sr_movieAudio );

    //! tallest the encoded video may be. Capturing scales down to this, which
    //! keeps Retina/4K windows from producing enormous files (and enormous
    //! readbacks). 0 means native: use the window size, up to the hardware limit.
    int sr_movieMaxHeight = 1080;
    static tConfItem<int> sr_movieMaxHeightConf( "MOVIE_MAX_HEIGHT", sr_movieMaxHeight,
                                                 [](const int& v){ return v >= 0; } );

    //! 0 = H.264, 1 = HEVC. HEVC's hardware encoder handles native frame sizes
    //! (H.264's caps at 4096 on a side), makes smaller files, and is the only one
    //! whose rate control actually honours the Quality setting on this platform.
    int sr_movieCodec = 1;
    static tConfItem<int> sr_movieCodecConf( "MOVIE_CODEC", sr_movieCodec,
                                             [](const int& v){ return v >= 0 && v <= 1; } );

    //! keep a rolling instant-replay buffer of the last few seconds. The frames
    //! are encoded as they are captured and kept in memory, so saving a clip is a
    //! quick mux with no re-encoding. On by default so the clip key works out of
    //! the box; turn it off in the menu to save the encoder's cost.
    bool sr_movieBuffer = true;
    static tConfItem<bool> sr_movieBufferConf( "MOVIE_BUFFER", sr_movieBuffer );

    //! how long a saved clip is
    int sr_movieClipSeconds = 30;
    static tConfItem<int> sr_movieClipSecondsConf( "MOVIE_CLIP_SECONDS", sr_movieClipSeconds,
                                                   [](const int& v){ return v > 0; } );

    //! video quality, which picks how many bits per pixel per frame to spend. It
    //! drives the encoder's bitrate for both clips and takes; 0 low, 1 medium,
    //! 2 high, 3 ultra.
    int sr_movieQuality = 1;
    static tConfItem<int> sr_movieQualityConf( "MOVIE_QUALITY", sr_movieQuality,
                                               [](const int& v){ return v >= 0 && v <= 3; } );

    //! how big the clip's worth of buffered history may get, in MB. The rolling
    //! buffer is sized to hold the clip window at the current quality, so this is
    //! the memory the instant replay is allowed to use.
    int sr_movieBufferMaxMB = 50;
    static tConfItem<int> sr_movieBufferMaxMBConf( "MOVIE_BUFFER_MAX_MB", sr_movieBufferMaxMB,
                                                   [](const int& v){ return v > 0; } );

    int MovieKeyframeSeconds()
    {
        return 2;   // a keyframe every two seconds
    }

    int MovieAudioBitrateBps()
    {
        return 128000;
    }

    static int MovieBitsPerPixelMilli()
    {
        switch ( sr_movieQuality )
        {
        case 0:  return 60;    // low
        case 2:  return 240;   // high
        case 3:  return 400;   // ultra
        default: return 120;   // medium
        }
    }

    int MovieVideoBitrateBps()
    {
        // bits per pixel per frame, scaled by the captured resolution and rate.
        // The output height the recorder will actually use, so the bitrate follows
        // the resolution instead of being pinned to a reference size.
        int outHeight = sr_movieMaxHeight;
        if ( outHeight <= 0 )
            outHeight = 1440;   // native-ish; the encoder caps it to the hardware limit
        // 16:10 is what a Retina display recorded at native gives; close enough
        double const pixels = (double)outHeight * outHeight * 16.0 / 10.0;

        int const fps = sr_movieFPS > 0 ? sr_movieFPS : 60;
        double bps = pixels * fps * MovieBitsPerPixelMilli() / 1000.0;
        if ( bps < 1000000.0 ) bps = 1000000.0;
        if ( bps > 150000000.0 ) bps = 150000000.0;
        return (int)bps;
    }

    int MovieBufferSeconds()
    {
        // a clip needs its own length plus one keyframe interval of history so a
        // keyframe at or before the start is always present, plus a small margin
        return sr_movieClipSeconds + MovieKeyframeSeconds() + 2;
    }

    long long MovieBufferMaxBytes()
    {
        // Target size is the clip's worth of buffered history, but the buffer has
        // to hold the whole clip window plus a keyframe of margin, not just the
        // exported clip. Size it from the window length so the target still reads
        // as "how much memory the replay may use".
        double const windowScale = (double)MovieBufferSeconds()
                                 / ( sr_movieClipSeconds > 0 ? sr_movieClipSeconds : 30 );
        double bytes = (double)sr_movieBufferMaxMB * 1024.0 * 1024.0 * windowScale;
        return (long long)bytes;
    }

#ifndef DEDICATED
    //! state of the take in progress
    static bool    sr_active      = false;   //!< a take is in progress
    static bool    sr_started     = false;   //!< the platform encoder is up
    static int     sr_startedW    = 0;
    static int     sr_startedH    = 0;
    static double  sr_startTime   = 0;
    static double  sr_nextCapture = 0;
    static double  sr_lastWall    = 0;   //!< last capture attempt, to spot a pause
    static tString sr_pendingPath;

    //! state of the rolling replay buffer
    static bool    sr_bufferStarted   = false;  //!< the platform buffer encoder is up
    static int     sr_bufferW         = 0;
    static int     sr_bufferH         = 0;
    static double  sr_bufferStartTime = 0;
    static double  sr_bufferNext      = 0;
    static double  sr_bufferLastWall  = 0;   //!< last capture attempt, to spot a pause
    //! true after the encoder failed to start, so we stop retrying every frame
    //! without touching the player's MOVIE_BUFFER setting
    static bool    sr_bufferUnavailable = false;
    static bool    sr_bufferSettingSeen = false;

    static int sr_audioFrequency = 0;   //!< 0 means the mixer is not running
    static int sr_audioChannels  = 0;

    //! result of a background clip save, printed by the render thread
    static std::mutex sr_clipReportMutex;
    static bool sr_clipReportPending = false;
    static bool sr_clipReportOk = false;
    static tString sr_clipReportPath;
    static long long sr_clipReportBytes = 0;

    //! Single producer (the audio thread) / single consumer (the render thread).
    //! Both indices only ever move in whole frames, so the ring never splits one.
    static size_t const sr_audioRingBytes = 1 << 19;   // 512 KiB, ~3 s stereo at 44.1 kHz
    static unsigned char sr_audioRing[sr_audioRingBytes];
    static std::atomic<size_t> sr_audioWrite( 0 );
    static std::atomic<size_t> sr_audioRead( 0 );
    static long long sr_audioSent = 0;                 //!< bytes handed to the encoder
#endif

    void SetAudioFormat( int frequency, int channels )
    {
#ifndef DEDICATED
        sr_audioFrequency = frequency;
        sr_audioChannels  = channels;
#endif
    }

    bool AudioWanted()
    {
#ifndef DEDICATED
        return ( sr_active || sr_bufferStarted ) && sr_movieAudio
               && sr_audioFrequency > 0 && sr_audioChannels > 0
               && sr_RecorderPlatformHasAudio();
#else
        return false;
#endif
    }

    void OnAudioSamples( const unsigned char * data, int bytes )
    {
#ifndef DEDICATED
        if ( bytes <= 0 || !AudioWanted() )
            return;

        size_t const write = sr_audioWrite.load( std::memory_order_relaxed );
        size_t const read  = sr_audioRead.load( std::memory_order_acquire );

        // rather lose a block than stall the audio thread
        if ( write + bytes - read > sr_audioRingBytes )
            return;

        size_t const offset = write % sr_audioRingBytes;
        size_t const first  = std::min( (size_t)bytes, sr_audioRingBytes - offset );
        memcpy( sr_audioRing + offset, data, first );
        if ( first < (size_t)bytes )
            memcpy( sr_audioRing, data + first, (size_t)bytes - first );

        sr_audioWrite.store( write + bytes, std::memory_order_release );
#endif
    }

#ifndef DEDICATED
    //! hands mixed audio to the encoder; called on the render thread, so it may
    //! take its time and leave data behind if the encoder is busy
    static void DrainAudio()
    {
        if ( !AudioWanted() )
            return;

        int const frameBytes = sr_audioChannels * 2;
        size_t write = sr_audioWrite.load( std::memory_order_acquire );
        size_t read  = sr_audioRead.load( std::memory_order_relaxed );

        while ( write - read >= (size_t)frameBytes )
        {
            size_t const offset = read % sr_audioRingBytes;
            size_t avail = std::min( write - read, sr_audioRingBytes - offset );
            avail = std::min( avail, (size_t)16384 );
            avail -= avail % frameBytes;          // whole frames only
            if ( avail == 0 )
                break;

            double const seconds = (double)sr_audioSent
                                 / ( (double)sr_audioFrequency * sr_audioChannels * 2 );
            if ( !sr_RecorderPlatformAudio( sr_audioRing + offset, (int)avail, seconds ) )
                break;                            // offer it again next frame

            sr_audioSent += avail;
            read += avail;
        }

        sr_audioRead.store( read, std::memory_order_release );
    }
#endif

#ifndef DEDICATED
    //! expands a leading ~ to the home directory
    static tString ExpandPath( tString const & path )
    {
        if ( path.Len() > 1 && path[0] == '~' )
        {
            char const * home = getenv( "HOME" );
            if ( home )
                return tString( home ) + path.SubStr( 1 );
        }
        return path;
    }

    //! creates the folder and any missing parents
    static void MakeFolders( tString const & path )
    {
        for ( int i = 1; i < static_cast<int>( path.Len() ) - 1; ++i )
        {
            if ( path[i] == '/' )
                sr_mkdir( path.SubStr( 0, i ) );
        }
        sr_mkdir( path );
    }

    static tString NewFileName()
    {
        time_t raw = time( NULL );
        char stamp[32];
        strftime( stamp, sizeof stamp, "%Y-%m-%d_%H-%M-%S", localtime( &raw ) );
        return tString( "recording_" ) + stamp + ".mp4";
    }

    static tString NewClipName()
    {
        time_t raw = time( NULL );
        char stamp[32];
        strftime( stamp, sizeof stamp, "%Y-%m-%d_%H-%M-%S", localtime( &raw ) );
        return tString( "clip_" ) + stamp + ".mp4";
    }

    static long long FileSize( tString const & path )
    {
#ifdef WIN32
        return 0;
#else
        struct stat info;
        if ( stat( path.c_str(), &info ) == 0 )
            return static_cast<long long>( info.st_size );
        return 0;
#endif
    }
#endif

    void Start()
    {
#ifndef DEDICATED
        if ( sr_active )
            return;

        // a take and the rolling buffer share one capture path, so a take pauses
        // the buffer; it resumes on the next frame after the take stops
        if ( sr_bufferStarted )
        {
            sr_RecorderPlatformBufferEnd();
            sr_bufferStarted = false;
        }

        tString const dir = ExpandPath( sr_movieDir );
        MakeFolders( dir );

        sr_pendingPath = dir + "/" + NewFileName();
        sr_active      = true;
        sr_started     = false;   // the encoder is created on the first frame

        sr_audioRead.store( sr_audioWrite.load() );   // drop stale mix
        sr_audioSent = 0;

        if ( sr_movieAudio && ( sr_audioFrequency <= 0 || sr_audioChannels <= 0 ) )
            con << "Recording: no audio available; set Sound Quality above Off in the sound menu\n";

        con << "Recording " << sr_pendingPath << "\n";
#endif
    }

    void Stop()
    {
#ifndef DEDICATED
        if ( !sr_active )
            return;

        // flush the tail of the audio before closing the file
        DrainAudio();

        double seconds = 0;
        if ( sr_started )
            seconds = sr_RecorderPlatformEnd();

        sr_active  = false;
        sr_started = false;

        // report what was written, so it is obvious the take is complete
        long long const bytes = FileSize( sr_pendingPath );
        double const megabytes = static_cast<double>( bytes ) / ( 1024.0 * 1024.0 );

        std::ostringstream message;
        message << "Recording saved: " << sr_pendingPath;

        if ( seconds > 0 || bytes > 0 )
        {
            message << "  (" << std::fixed << std::setprecision( 1 );
            if ( seconds > 0 )
                message << seconds << " s, ";
            message << megabytes << " MB)";
        }

        con << message.str() << "\n";
#endif
    }

    void Toggle()
    {
#ifndef DEDICATED
        if ( sr_active )
            Stop();
        else
            Start();
#endif
    }

    void Shutdown()
    {
#ifndef DEDICATED
        // Finalise any take or buffer while the GL context is still alive: the
        // encoder releases its GL readback objects, and leaving an AVAssetWriter
        // or VTCompressionSession (and its VideoToolbox threads) running into
        // static destruction crashes.
        if ( sr_active )
            Stop();
        if ( sr_bufferStarted )
        {
            sr_RecorderPlatformBufferEnd();
            sr_bufferStarted = false;
        }
#endif
    }

    void SaveClip( double seconds )
    {
#ifndef DEDICATED
        if ( !sr_bufferStarted )
        {
            con << "No replay buffer running; turn on Instant Replay first\n";
            return;
        }

        if ( seconds <= 0 )
            seconds = sr_movieClipSeconds;
        if ( seconds > MovieBufferSeconds() )
            seconds = MovieBufferSeconds();

        tString const dir = ExpandPath( sr_movieDir );
        MakeFolders( dir );
        tString const path = dir + "/" + NewClipName();

        // the backend muxes in the background, so pressing the key never stalls
        // the game; the result is reported by OnClipSaved a moment later
        if ( !sr_RecorderPlatformClip( seconds, path ) )
        {
            con << "Clip could not be saved\n";
            return;
        }

        con << "Saving clip...\n";
#endif
    }

    void OnClipSaved( bool ok, tString const & path, long long bytes )
    {
#ifndef DEDICATED
        std::lock_guard<std::mutex> lock( sr_clipReportMutex );
        sr_clipReportPending = true;
        sr_clipReportOk      = ok;
        sr_clipReportPath    = path;
        sr_clipReportBytes   = bytes;
#endif
    }

    void OnFrame( int width, int height )
    {
#ifndef DEDICATED
        if ( width <= 0 || height <= 0 )
            return;

        // report a clip that finished muxing on the background thread
        {
            std::lock_guard<std::mutex> lock( sr_clipReportMutex );
            if ( sr_clipReportPending )
            {
                sr_clipReportPending = false;
                if ( sr_clipReportOk )
                    con << "Clip saved: " << sr_clipReportPath << "  ("
                        << std::fixed << std::setprecision( 1 )
                        << static_cast<double>( sr_clipReportBytes ) / ( 1024.0 * 1024.0 ) << " MB)\n";
                else
                    con << "Clip failed: " << sr_clipReportPath << "\n";
            }
        }

        // A change to the setting means the player acted: allow a fresh attempt
        // even if the encoder failed before, and never flip the setting itself.
        if ( sr_movieBuffer != sr_bufferSettingSeen )
        {
            sr_bufferSettingSeen = sr_movieBuffer;
            sr_bufferUnavailable = false;
        }

        // Turning the rolling buffer off releases its encoder and ring. A take
        // and the buffer are mutually exclusive: they share the capture path.
        if ( !sr_movieBuffer && sr_bufferStarted )
        {
            sr_RecorderPlatformBufferEnd();
            sr_bufferStarted = false;
        }

        if ( sr_active )
        {
            // ---- take recording ----
            if ( !sr_started )
            {
                bool const withAudio = sr_movieAudio && sr_audioFrequency > 0 && sr_audioChannels > 0;
                if ( !sr_RecorderPlatformBegin( sr_pendingPath, width, height,
                                                withAudio ? sr_audioFrequency : 0,
                                                withAudio ? sr_audioChannels : 0 ) )
                {
                    con << "Recording could not start\n";
                    sr_active = false;
                    return;
                }

                sr_started     = true;
                sr_startedW    = width;
                sr_startedH    = height;
                sr_startTime   = tRealSysTimeFloat();
                sr_nextCapture = 0;
                sr_lastWall    = 0;
            }

            // A size change (window resize, fullscreen toggle) would need the
            // encoder rebuilt; ending the take cleanly is simpler.
            if ( width != sr_startedW || height != sr_startedH )
            {
                con << "Resolution changed, recording stopped\n";
                Stop();
                return;
            }

            // restart the capture clock after a pause (hidden window) so the take
            // does not try to make up missed frames; see the buffer path below
            double const wall = tRealSysTimeFloat();
            if ( sr_lastWall > 0 && wall - sr_lastWall > 1.0 )
            {
                sr_startTime = wall;
                sr_nextCapture = 0;
            }
            sr_lastWall = wall;

            double const now = tRealSysTimeFloat() - sr_startTime;
            if ( now < sr_nextCapture )
                return;

            int const fps = sr_movieFPS > 0 ? sr_movieFPS : 60;
            sr_nextCapture = now + 1.0 / fps;

            sr_RecorderPlatformFrame( now );
            DrainAudio();

            // If the encoder failed mid-take, stop cleanly now rather than trying
            // to finalise a broken writer on the way out.
            if ( !sr_RecorderPlatformIsHealthy() )
            {
                con << "Recording stopped: the video encoder failed\n";
                Stop();
            }
            return;
        }

        if ( sr_movieBuffer && !sr_bufferUnavailable )
        {
            // ---- rolling replay buffer ----
            if ( !sr_bufferStarted )
            {
                bool const withAudio = sr_movieAudio && sr_audioFrequency > 0 && sr_audioChannels > 0;
                if ( !sr_RecorderPlatformBufferBegin( width, height,
                                                      withAudio ? sr_audioFrequency : 0,
                                                      withAudio ? sr_audioChannels : 0 ) )
                {
                    con << "Instant replay could not start\n";
                    sr_bufferUnavailable = true;
                    return;
                }

                sr_bufferStarted   = true;
                sr_bufferW         = width;
                sr_bufferH         = height;
                sr_bufferStartTime = tRealSysTimeFloat();
                sr_bufferNext      = 0;
                sr_bufferLastWall  = 0;

                sr_audioRead.store( sr_audioWrite.load() );   // drop stale mix
                sr_audioSent = 0;
            }

            if ( width != sr_bufferW || height != sr_bufferH )
            {
                con << "Resolution changed, replay buffer restarted\n";
                sr_RecorderPlatformBufferEnd();
                sr_bufferStarted = false;
                return;
            }

            // If the window was hidden, the throttle deadline is in the past and
            // the capture clock has moved on. Restart the clock from now so the
            // recorder does not try to make up the missed frames, which is what
            // made the framerate take seconds to recover.
            double const wall = tRealSysTimeFloat();
            if ( sr_bufferLastWall > 0 && wall - sr_bufferLastWall > 1.0 )
            {
                sr_bufferStartTime = wall;
                sr_bufferNext = 0;
            }
            sr_bufferLastWall = wall;

            double const now = tRealSysTimeFloat() - sr_bufferStartTime;
            if ( now < sr_bufferNext )
                return;

            int const fps = sr_movieFPS > 0 ? sr_movieFPS : 60;
            sr_bufferNext = now + 1.0 / fps;

            sr_RecorderPlatformFrame( now );
            DrainAudio();

            if ( !sr_RecorderPlatformIsHealthy() )
            {
                con << "Instant replay stopped: the video encoder failed\n";
                sr_RecorderPlatformBufferEnd();
                sr_bufferStarted = false;
                sr_bufferUnavailable = true;
            }
        }
#endif
    }
}
