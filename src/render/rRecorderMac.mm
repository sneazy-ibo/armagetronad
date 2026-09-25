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

//! macOS implementation of the recorder.
//!
//! One frame-capture path serves both modes: an AVAssetWriter (take recording,
//! streamed straight to disk) and a VTCompressionSession (instant replay, kept
//! compressed in a ring in memory). The captured frame is scaled down on the GPU
//! to MOVIE_MAX_HEIGHT before it is read back.
//!
//! Saving a clip is a pass-through mux of the already-encoded samples from the
//! last keyframe, so it does not re-encode and does not touch the render thread's
//! budget.
//!
//! Note: compiled without ARC (the build passes CXXFLAGS through as
//! OBJCXXFLAGS), so Objective-C objects are managed by hand here.

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#include "rRecorder.h"

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>

#include <dispatch/dispatch.h>

#include "rScreen.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

#include "rConsole.h"
#include "tSysTime.h"

//! Number of readback buffers, and how many captures behind we consume them.
//! Reading a frame while the GPU is still rendering it is what makes a capture
//! stall, so a frame is written out several capture intervals after it was
//! queued: with 6 buffers and 3 frames of slack the GPU has ~50 ms at 60 fps.
static int const sr_ringSize = 6;
static int const sr_ringLatency = 3;

enum SrMode { SR_NONE, SR_TAKE, SR_BUFFER };
static SrMode sr_mode = SR_NONE;

// ---------------------------------------------------------------------------
// Shared frame capture state
// ---------------------------------------------------------------------------
static int   sr_width = 0;         //!< captured (window) size
static int   sr_height = 0;
static int   sr_outW = 0;          //!< encoded size after MOVIE_MAX_HEIGHT scaling
static int   sr_outH = 0;

static GLuint sr_pbo[sr_ringSize] = { 0, 0, 0, 0, 0, 0 };
static GLuint sr_captureTexture = 0;   //!< full window resolution
static GLuint sr_encodeTexture = 0;    //!< scaled to sr_outW x sr_outH
static GLuint sr_captureFbo = 0;       //!< has sr_encodeTexture attached
static int    sr_mipLevels = 0;        //!< mip levels generated for the downscale
static bool   sr_glReady = false;
static int    sr_issued = 0;        //!< how many readbacks have been started
static int    sr_pendingSlot[sr_ringSize] = { -1, -1, -1, -1, -1, -1 };
static double sr_pendingTime[sr_ringSize] = { 0, 0, 0, 0, 0, 0 };
static int    sr_pendingCount = 0;
static int    sr_pendingConsumed = 0;
static long   sr_dropped = 0;
static bool   sr_failed = false;    //!< the encoder failed; stop capturing

// ---------------------------------------------------------------------------
// Take recording state (AVAssetWriter)
// ---------------------------------------------------------------------------
static AVAssetWriter *sr_writer = nil;
static AVAssetWriterInput *sr_input = nil;
static AVAssetWriterInputPixelBufferAdaptor *sr_adaptor = nil;
static AVAssetWriterInput *sr_audioInput = nil;
static int sr_audioFrequency = 0;
static int sr_audioChannels = 0;
static double sr_lastStamp = 0;    //!< capture time of the last frame written

// ---------------------------------------------------------------------------
// Instant replay state (VTCompressionSession + ring of encoded samples)
// ---------------------------------------------------------------------------
struct sr_BufSample
{
    CMSampleBufferRef buf;
    double            pts;
    bool              keyframe;
    size_t            bytes;
};

static VTCompressionSessionRef sr_session = NULL;
static CVPixelBufferPoolRef sr_bufferPool = NULL;
static std::deque<sr_BufSample> sr_buffer;      //!< encoded samples, oldest first
static std::mutex sr_bufferMutex;               //!< guards sr_buffer and its size
static size_t sr_bufferBytes = 0;
static double sr_bufferNewest = 0;              //!< pts of the newest sample
static int sr_bufferFPS = 60;                   //!< capture rate, for frame durations
static size_t sr_bufferMaxBytes = 0;            //!< memory cap, set at buffer start
static double sr_bufferSeconds = 30;            //!< history to keep, set at buffer start

//! Rolling PCM for the replay buffer's audio, delivered on the render thread by
//! sr_RecorderPlatformAudio, so no lock is needed. It is encoded to AAC only when
//! a clip is saved.
struct sr_AudioChunk
{
    std::vector<unsigned char> data;
    double                     seconds;   // start time on the buffer's timeline
};
static std::deque<sr_AudioChunk> sr_audioChunks;
static size_t sr_audioChunkBytes = 0;
static size_t sr_audioMaxBytes = 0;

static CMSampleBufferRef sr_CreateAudioSample( const unsigned char * data, int bytes,
                                               double seconds, int freq, int channels );

// ---------------------------------------------------------------------------
// Crash breadcrumbs: a last-resort record of a failed save, written to stderr
// and, when ARMAGETRON_RECORDER_LOG is set, to that file.
// ---------------------------------------------------------------------------
static void sr_RecorderBreadcrumb( char const * what )
{
    std::cerr << "rRecorder: " << what << std::endl;
    char const * path = getenv( "ARMAGETRON_RECORDER_LOG" );
    if ( path )
    {
        FILE * f = fopen( path, "a" );
        if ( f )
        {
            fprintf( f, "rRecorder: %s\n", what );
            fclose( f );
        }
    }
}

static void sr_RecorderUncaughtException( NSException * e )
{
    std::ostringstream s;
    s << "uncaught Objective-C exception: "
      << ( e.name ? e.name.UTF8String : "?" ) << " - "
      << ( e.reason ? e.reason.UTF8String : "?" );
    sr_RecorderBreadcrumb( s.str().c_str() );
}

static void sr_RecorderTerminate()
{
    sr_RecorderBreadcrumb( "std::terminate" );
    abort();
}

static void sr_InstallCrashHandlers()
{
    static bool done = false;
    if ( done )
        return;
    done = true;
    NSSetUncaughtExceptionHandler( &sr_RecorderUncaughtException );
    std::set_terminate( &sr_RecorderTerminate );
}

static bool sr_IsKeyframe( CMSampleBufferRef sb )
{
    CFArrayRef atts = CMSampleBufferGetSampleAttachmentsArray( sb, true );
    if ( !atts || CFArrayGetCount( atts ) == 0 )
        return true;
    CFDictionaryRef d = (CFDictionaryRef)CFArrayGetValueAtIndex( atts, 0 );
    return !CFDictionaryContainsKey( d, kCMSampleAttachmentKey_NotSync );
}

// ---------------------------------------------------------------------------
// GL capture objects, shared by both modes
// ---------------------------------------------------------------------------
static void sr_DeleteGLObjects()
{
    if ( !sr_glReady )
        return;

    if ( sr_pbo[0] )
    {
        glDeleteBuffers( sr_ringSize, sr_pbo );
        for ( int i = 0; i < sr_ringSize; ++i ) sr_pbo[i] = 0;
    }
    if ( sr_captureFbo )
    {
        glDeleteFramebuffersEXT( 1, &sr_captureFbo );
        sr_captureFbo = 0;
    }
    if ( sr_captureTexture )
    {
        glDeleteTextures( 1, &sr_captureTexture );
        sr_captureTexture = 0;
    }
    if ( sr_encodeTexture )
    {
        glDeleteTextures( 1, &sr_encodeTexture );
        sr_encodeTexture = 0;
    }
    sr_glReady = false;
}

static void sr_CreateGLObjects( int width, int height, int outW, int outH )
{
    sr_width  = width;
    sr_height = height;
    sr_outW   = outW;
    sr_outH   = outH;
    sr_issued = 0;
    sr_pendingCount = 0;
    sr_pendingConsumed = 0;
    for ( int i = 0; i < sr_ringSize; ++i ) sr_pendingSlot[i] = -1;

    glGenBuffers( sr_ringSize, sr_pbo );
    for ( int i = 0; i < sr_ringSize; ++i )
    {
        glBindBuffer( GL_PIXEL_PACK_BUFFER, sr_pbo[i] );
        glBufferData( GL_PIXEL_PACK_BUFFER, (GLsizeiptr)sr_outW * sr_outH * 4,
                      NULL, GL_STREAM_READ );
    }
    glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

    // full window resolution, the source of the downscale. Mipmapped so the
    // minification filters properly instead of aliasing: thin Tron lines would
    // otherwise crawl and shimmer in the recording.
    int const scale = ( sr_outW > 0 && sr_outH > 0 )
                    ? std::max( ( sr_width + sr_outW - 1 ) / sr_outW,
                                ( sr_height + sr_outH - 1 ) / sr_outH )
                    : 1;
    sr_mipLevels = 0;
    while ( scale > ( 1 << sr_mipLevels ) )
        ++sr_mipLevels;

    glGenTextures( 1, &sr_captureTexture );
    glBindTexture( GL_TEXTURE_2D, sr_captureTexture );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, sr_width, sr_height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, sr_mipLevels );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     sr_mipLevels > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture( GL_TEXTURE_2D, 0 );

    // encoded resolution, the target of the downscale and the readback
    glGenTextures( 1, &sr_encodeTexture );
    glBindTexture( GL_TEXTURE_2D, sr_encodeTexture );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, sr_outW, sr_outH, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glBindTexture( GL_TEXTURE_2D, 0 );

    glGenFramebuffersEXT( 1, &sr_captureFbo );
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, sr_captureFbo );
    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                               GL_TEXTURE_2D, sr_encodeTexture, 0 );
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

    sr_glReady = true;
}

//! the pixel buffer pool the current mode fills
static CVPixelBufferPoolRef sr_CurrentPool()
{
    if ( sr_mode == SR_BUFFER )
        return sr_bufferPool;
    if ( sr_mode == SR_TAKE && sr_adaptor )
        return sr_adaptor.pixelBufferPool;
    return NULL;
}

//! hands a captured pixel buffer to the mode's encoder
static bool sr_Deliver( CVPixelBufferRef buffer, double seconds )
{
    if ( sr_mode == SR_BUFFER )
    {
        if ( !sr_session )
            return false;
        CMTime const pts = CMTimeMakeWithSeconds( seconds, 600 );
        CMTime const dur = CMTimeMake( 1, sr_bufferFPS );
        if ( VTCompressionSessionEncodeFrame( sr_session, buffer, pts, dur,
                                              NULL, NULL, NULL ) != noErr )
        {
            sr_failed = true;
            return false;
        }
        return true;
    }

    if ( sr_mode == SR_TAKE )
    {
        // appendPixelBuffer raises if the input is not ready, so check first and
        // never let an exception escape into the game loop
        if ( !sr_adaptor || !sr_input || !sr_input.readyForMoreMediaData )
        {
            ++sr_dropped;
            return false;
        }
        CMTime const time = CMTimeMakeWithSeconds( seconds, 600 );
        @try
        {
            if ( ![sr_adaptor appendPixelBuffer:buffer withPresentationTime:time] )
            {
                ++sr_dropped;
                if ( sr_writer && sr_writer.status == AVAssetWriterStatusFailed )
                    sr_failed = true;
                return false;
            }
        }
        @catch ( NSException *e )
        {
            sr_RecorderBreadcrumb( "take: appendPixelBuffer raised" );
            ++sr_dropped;
            if ( sr_writer && sr_writer.status == AVAssetWriterStatusFailed )
                sr_failed = true;
            return false;
        }
        sr_lastStamp = seconds;
        return true;
    }

    return false;
}

//! true when the capture side is idle: the window is hidden, minimised or
//! otherwise not presenting frames. While that is the case the recorder skips
//! entirely, so it has no stale pixel buffers or encoder backlog to work off
//! when the window comes back.
static bool sr_CaptureIdle()
{
    if ( !sr_screen )
        return true;
    Uint32 const flags = SDL_GetWindowFlags( sr_screen );
    return ( flags & SDL_WINDOW_SHOWN ) == 0
        || ( flags & SDL_WINDOW_MINIMIZED ) != 0
        || ( flags & SDL_WINDOW_HIDDEN ) != 0;
}

//! drops every queued readback and resets the timing, so a pause in capture does
//! not leave work behind. Must be called with the GL context current.
static void sr_ResetCapture()
{
    for ( int i = 0; i < sr_ringSize; ++i )
        sr_pendingSlot[i] = -1;
    sr_pendingCount = 0;
    sr_pendingConsumed = 0;
    sr_issued = 0;
}

//! takes the frame sitting in a readback buffer, copies it into a pooled pixel
//! buffer the right way up, and hands it to the encoder
static void sr_RecorderConsume( int slot, double seconds )
{
    CVPixelBufferPoolRef pool = sr_CurrentPool();
    if ( slot < 0 || !pool )
        return;

    CVPixelBufferRef buffer = NULL;
    CVPixelBufferPoolCreatePixelBuffer( NULL, pool, &buffer );
    if ( !buffer )
        return;

    if ( CVPixelBufferLockBaseAddress( buffer, 0 ) == kCVReturnSuccess )
    {
        glBindBuffer( GL_PIXEL_PACK_BUFFER, sr_pbo[slot] );
        void *source = glMapBuffer( GL_PIXEL_PACK_BUFFER, GL_READ_ONLY );
        if ( source )
        {
            unsigned char *base = (unsigned char *)CVPixelBufferGetBaseAddress( buffer );
            size_t const stride = CVPixelBufferGetBytesPerRow( buffer );

            // The framebuffer's origin is bottom-left and video's is top-left, so
            // rows are copied in reverse: the flip costs nothing extra because the
            // data has to be copied anyway.
            for ( int row = 0; row < sr_outH; ++row )
            {
                memcpy( base + (size_t)row * stride,
                        (unsigned char *)source + (size_t)( sr_outH - 1 - row ) * sr_outW * 4,
                        (size_t)sr_outW * 4 );
            }
            glUnmapBuffer( GL_PIXEL_PACK_BUFFER );
        }
        glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
        CVPixelBufferUnlockBaseAddress( buffer, 0 );

        sr_Deliver( buffer, seconds );
    }
    CVPixelBufferRelease( buffer );
}

//! VideoToolbox hands each freshly encoded sample here on its own thread.
static void sr_EncodedFrame( void *, void *, OSStatus status, VTEncodeInfoFlags,
                             CMSampleBufferRef sb )
{
    if ( status != noErr || !sb )
        return;

    sr_BufSample s;
    s.buf      = (CMSampleBufferRef)CFRetain( sb );
    s.keyframe = sr_IsKeyframe( sb );
    s.pts      = CMTimeGetSeconds( CMSampleBufferGetPresentationTimeStamp( sb ) );
    s.bytes    = (size_t)CMSampleBufferGetTotalSampleSize( sb );

    std::lock_guard<std::mutex> lock( sr_bufferMutex );
    sr_bufferBytes += s.bytes;
    sr_bufferNewest = s.pts;
    sr_buffer.push_back( s );

    // keep MovieBufferSeconds() of history, plus a little slack so a keyframe at
    // or before the start of the requested window is still there
    double const cutoff = sr_bufferNewest - sr_bufferSeconds - 2.0;
    while ( sr_buffer.size() > 2 && sr_buffer[1].pts < cutoff )
    {
        sr_bufferBytes -= sr_buffer.front().bytes;
        CFRelease( sr_buffer.front().buf );
        sr_buffer.pop_front();
    }

    // and never grow past the memory cap
    while ( sr_bufferBytes > sr_bufferMaxBytes && sr_buffer.size() > 2 )
    {
        sr_bufferBytes -= sr_buffer.front().bytes;
        CFRelease( sr_buffer.front().buf );
        sr_buffer.pop_front();
    }
}

static void sr_SetupCaptureSize( int width, int height, int & outW, int & outH )
{
    outW = width;
    outH = height;

    // MOVIE_MAX_HEIGHT caps the output; 0 means native (the full drawable).
    int const maxHeight = rRecorder::sr_movieMaxHeight;
    if ( maxHeight > 0 && height > maxHeight )
    {
        outH = maxHeight;
        outW = (int)( (long)width * outH / height );
    }

    // Hardware encoders cap at 4096 on a side for H.264 but much higher for
    // HEVC; going over drops to a software encoder and tanks the framerate.
    int const hwMax = ( rRecorder::sr_movieCodec == 1 ) ? 8192 : 4096;
    if ( outW > hwMax || outH > hwMax )
    {
        double const scale = (double)hwMax / ( outW > outH ? outW : outH );
        outW = (int)( outW * scale );
        outH = (int)( outH * scale );
    }

    outW &= ~1;
    outH &= ~1;
    if ( outW < 2 ) outW = 2;
    if ( outH < 2 ) outH = 2;
}

// ---------------------------------------------------------------------------
// Take recording
// ---------------------------------------------------------------------------
bool sr_RecorderPlatformBegin( tString const & path, int width, int height,
                               int audioFrequency, int audioChannels )
{
    @autoreleasepool
    {
        sr_InstallCrashHandlers();

        sr_failed = false;
        sr_dropped = 0;
        sr_lastStamp = 0;
        sr_mode = SR_TAKE;

        int outW = 0, outH = 0;
        sr_SetupCaptureSize( width, height, outW, outH );

        NSString *file = [NSString stringWithUTF8String:path.c_str()];
        NSError *error = nil;
        sr_writer = [[AVAssetWriter alloc] initWithURL:[NSURL fileURLWithPath:file]
                                              fileType:AVFileTypeMPEG4
                                                 error:&error];
        if ( !sr_writer )
        {
            con << "Recording: cannot write " << path << ": "
                << ( error ? error.localizedDescription.UTF8String : "unknown error" ) << "\n";
            sr_mode = SR_NONE;
            return false;
        }

        // the bitrate is the chosen quality, scaled to the captured size and rate
        int const fps = rRecorder::sr_movieFPS > 0 ? rRecorder::sr_movieFPS : 60;
        long const capped = rRecorder::MovieVideoBitrateBps();
        bool const hevc = rRecorder::sr_movieCodec == 1;

        NSDictionary *compression = @{
            AVVideoAverageBitRateKey:       @( capped ),
            AVVideoMaxKeyFrameIntervalKey:  @( fps * rRecorder::MovieKeyframeSeconds() ),
            AVVideoProfileLevelKey:         hevc ? (__bridge NSString *)kVTProfileLevel_HEVC_Main_AutoLevel
                                                 : AVVideoProfileLevelH264HighAutoLevel,
            AVVideoAllowFrameReorderingKey: @YES,
        };
        NSDictionary *settings = @{
            AVVideoCodecKey:                 hevc ? AVVideoCodecTypeHEVC : AVVideoCodecTypeH264,
            AVVideoWidthKey:                 @( outW ),
            AVVideoHeightKey:                @( outH ),
            AVVideoCompressionPropertiesKey: compression,
            // tag the output as BT.709 so players do not guess the colour space
            // (a wrong guess is what makes exported colours look off)
            AVVideoColorPropertiesKey: @{
                AVVideoColorPrimariesKey:   AVVideoColorPrimaries_ITU_R_709_2,
                AVVideoTransferFunctionKey: AVVideoTransferFunction_ITU_R_709_2,
                AVVideoYCbCrMatrixKey:      AVVideoYCbCrMatrix_ITU_R_709_2,
            },
        };

        sr_input = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
                                                 outputSettings:settings];
        // never block the render thread: if the writer is behind, frames are dropped
        sr_input.expectsMediaDataInRealTime = YES;

        sr_adaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc]
            initWithAssetWriterInput:sr_input
       sourcePixelBufferAttributes:@{
            (id)kCVPixelBufferPixelFormatTypeKey:     @( kCVPixelFormatType_32BGRA ),
            (id)kCVPixelBufferWidthKey:               @( outW ),
            (id)kCVPixelBufferHeightKey:              @( outH ),
            (id)kCVPixelBufferIOSurfacePropertiesKey: @{},
        }];

        [sr_writer addInput:sr_input];

        // optional audio track; the sound system has already told us its format
        if ( audioFrequency > 0 && audioChannels > 0 )
        {
            NSDictionary *audioSettings = @{
                AVFormatIDKey:         @( kAudioFormatMPEG4AAC ),
                AVSampleRateKey:       @( audioFrequency ),
                AVNumberOfChannelsKey: @( audioChannels ),
                AVEncoderBitRateKey:   @( rRecorder::MovieAudioBitrateBps() ),
            };
            sr_audioInput = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio
                                                          outputSettings:audioSettings];
            sr_audioInput.expectsMediaDataInRealTime = YES;
            [sr_writer addInput:sr_audioInput];
            sr_audioFrequency = audioFrequency;
            sr_audioChannels  = audioChannels;
        }

        // fragments every couple of seconds keep a crashed take playable
        sr_writer.movieFragmentInterval = CMTimeMake( 2, 1 );

        if ( ![sr_writer startWriting] )
        {
            con << "Recording: startWriting failed: "
                << sr_writer.error.localizedDescription.UTF8String << "\n";
            [sr_audioInput release]; sr_audioInput = nil;
            [sr_adaptor release];    sr_adaptor = nil;
            [sr_input release];      sr_input = nil;
            [sr_writer release];     sr_writer = nil;
            sr_mode = SR_NONE;
            return false;
        }
        [sr_writer startSessionAtSourceTime:kCMTimeZero];

        sr_CreateGLObjects( width, height, outW, outH );
        return true;
    }
}

// ---------------------------------------------------------------------------
// Instant replay buffer
// ---------------------------------------------------------------------------
bool sr_RecorderPlatformBufferBegin( int width, int height,
                                     int audioFrequency, int audioChannels )
{
    @autoreleasepool
    {
        sr_InstallCrashHandlers();

        sr_failed = false;
        sr_dropped = 0;

        int outW = 0, outH = 0;
        sr_SetupCaptureSize( width, height, outW, outH );

        int const fps = rRecorder::sr_movieFPS > 0 ? rRecorder::sr_movieFPS : 60;
        sr_bufferFPS = fps;
        sr_bufferSeconds = rRecorder::MovieBufferSeconds();
        sr_bufferMaxBytes = (size_t)rRecorder::MovieBufferMaxBytes();
        long const capped = rRecorder::MovieVideoBitrateBps();
        bool const hevc = rRecorder::sr_movieCodec == 1;

        // audio is buffered as raw PCM and only encoded when a clip is saved
        sr_audioFrequency = audioFrequency;
        sr_audioChannels  = audioChannels;
        sr_audioChunks.clear();
        sr_audioChunkBytes = 0;
        sr_audioMaxBytes = (size_t)( (double)audioFrequency * audioChannels * 2
                                     * ( sr_bufferSeconds + 2 ) );
        if ( sr_audioMaxBytes == 0 )
            sr_audioMaxBytes = 1;

        OSStatus st = VTCompressionSessionCreate( kCFAllocatorDefault, outW, outH,
            hevc ? kCMVideoCodecType_HEVC : kCMVideoCodecType_H264,
            NULL, NULL, NULL, sr_EncodedFrame, NULL, &sr_session );
        if ( st != noErr || !sr_session )
        {
            con << "Instant replay: cannot create the encoder (" << (int)st << ")\n";
            sr_session = NULL;
            return false;
        }

        // Keyframes every couple of seconds keep a clip's start close to the
        // requested length. Frame reordering is off so samples stay in
        // presentation order, which keeps the pass-through mux simple.
        NSDictionary *props = @{
            (id)kVTCompressionPropertyKey_RealTime:           @YES,
            (id)kVTCompressionPropertyKey_AverageBitRate:     @( capped ),
            (id)kVTCompressionPropertyKey_MaxKeyFrameInterval: @( fps * rRecorder::MovieKeyframeSeconds() ),
            (id)kVTCompressionPropertyKey_ExpectedFrameRate:  @( fps ),
            (id)kVTCompressionPropertyKey_AllowFrameReordering: @NO,
            (id)kVTCompressionPropertyKey_ProfileLevel:       hevc ? (id)kVTProfileLevel_HEVC_Main_AutoLevel
                                                                   : (id)kVTProfileLevel_H264_High_AutoLevel,
            (id)kVTCompressionPropertyKey_ColorPrimaries:     (id)kCVImageBufferColorPrimaries_ITU_R_709_2,
            (id)kVTCompressionPropertyKey_TransferFunction:   (id)kCVImageBufferTransferFunction_ITU_R_709_2,
            (id)kVTCompressionPropertyKey_YCbCrMatrix:        (id)kCVImageBufferYCbCrMatrix_ITU_R_709_2,
        };
        VTSessionSetProperties( sr_session, (__bridge CFDictionaryRef)props );
        VTCompressionSessionPrepareToEncodeFrames( sr_session );

        NSDictionary *poolAttrs = @{
            (id)kCVPixelBufferPixelFormatTypeKey:      @( kCVPixelFormatType_32BGRA ),
            (id)kCVPixelBufferWidthKey:                @( outW ),
            (id)kCVPixelBufferHeightKey:               @( outH ),
            (id)kCVPixelBufferIOSurfacePropertiesKey:  @{},
            (id)kCVPixelBufferPoolMinimumBufferCountKey: @( sr_ringSize + 2 ),
        };
        if ( CVPixelBufferPoolCreate( NULL, NULL, (__bridge CFDictionaryRef)poolAttrs,
                                      &sr_bufferPool ) != kCVReturnSuccess )
        {
            con << "Instant replay: cannot create the pixel buffer pool\n";
            VTCompressionSessionInvalidate( sr_session );
            CFRelease( sr_session );
            sr_session = NULL;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock( sr_bufferMutex );
            for ( sr_BufSample &s : sr_buffer ) CFRelease( s.buf );
            sr_buffer.clear();
            sr_bufferBytes = 0;
            sr_bufferNewest = 0;
        }

        sr_CreateGLObjects( width, height, outW, outH );
        sr_mode = SR_BUFFER;
        return true;
    }
}

//! does the actual muxing. Runs on the clip queue, never on the render thread,
//! so a slow writer cannot stall the game.
static bool sr_MuxClip( std::vector<sr_BufSample> const & clip,
                        std::vector<sr_AudioChunk> const & audio,
                        double sessionStart, double endTime,
                        int freq, int channels, NSString *file )
{
    NSError *error = nil;
    AVAssetWriter *writer = [AVAssetWriter assetWriterWithURL:[NSURL fileURLWithPath:file]
                                                     fileType:AVFileTypeMPEG4
                                                        error:&error];
    if ( !writer )
    {
        sr_RecorderBreadcrumb( "clip: cannot create the writer" );
        return false;
    }

    // outputSettings:nil == pass-through: append compressed samples as-is.
    // A source format hint is required or canAddInput rejects the input.
    CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription( clip[0].buf );
    AVAssetWriterInput *input =
        [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                           outputSettings:nil
                                       sourceFormatHint:fmt];
    input.expectsMediaDataInRealTime = NO;
    if ( ![writer canAddInput:input] )
    {
        sr_RecorderBreadcrumb( "clip: the writer rejected the pass-through input" );
        return false;
    }
    [writer addInput:input];

    // optional audio track, encoded from the buffered PCM at save time
    AVAssetWriterInput *audioInput = nil;
    if ( freq > 0 && channels > 0 && !audio.empty() )
    {
        NSDictionary *audioSettings = @{
            AVFormatIDKey:         @( kAudioFormatMPEG4AAC ),
            AVSampleRateKey:       @( freq ),
            AVNumberOfChannelsKey: @( channels ),
            AVEncoderBitRateKey:   @( rRecorder::MovieAudioBitrateBps() ),
        };
        audioInput = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                                                      outputSettings:audioSettings];
        audioInput.expectsMediaDataInRealTime = NO;
        if ( [writer canAddInput:audioInput] )
            [writer addInput:audioInput];
        else
            audioInput = nil;
    }

    if ( ![writer startWriting] )
    {
        sr_RecorderBreadcrumb( "clip: startWriting failed" );
        return false;
    }
    [writer startSessionAtSourceTime:CMTimeMakeWithSeconds( sessionStart, 600 )];

    // prepare the audio samples that overlap the clip
    std::vector<CMSampleBufferRef> audioSamples;
    if ( audioInput )
    {
        int const frameBytes = channels * 2;
        for ( sr_AudioChunk const &c : audio )
        {
            double const chunkStart = c.seconds;
            double const chunkDur = (double)c.data.size() / ( (double)freq * frameBytes );
            if ( chunkStart + chunkDur <= sessionStart || chunkStart >= endTime )
                continue;

            unsigned char const * data = c.data.data();
            int bytes = (int)c.data.size();
            double start = chunkStart;

            // trim anything before the clip start, on whole frames
            if ( start < sessionStart )
            {
                long long skipFrames = (long long)( ( sessionStart - start ) * freq );
                long long skipBytes = skipFrames * frameBytes;
                if ( skipBytes >= bytes )
                    continue;
                data += skipBytes;
                bytes -= (int)skipBytes;
                start = sessionStart;
            }

            // and anything past the end
            long long roomFrames = (long long)( ( endTime - start ) * freq );
            long long roomBytes = roomFrames * frameBytes;
            if ( roomBytes >= 0 && roomBytes < bytes )
                bytes = (int)roomBytes;
            bytes -= bytes % frameBytes;
            if ( bytes <= 0 )
                continue;

            CMSampleBufferRef as = sr_CreateAudioSample( data, bytes, start, freq, channels );
            if ( as )
                audioSamples.push_back( as );
        }
    }

    // Feed both inputs whenever the writer is ready for them. Marking an input
    // finished as soon as its samples run out is important: otherwise the writer
    // keeps waiting for more of it and stops accepting the other input.
    bool ok = true;
    char const * failWhere = NULL;
    size_t vi = 0, ai = 0;
    long long spins = 0;
    bool videoDone = false, audioDone = audioSamples.empty();
    while ( !videoDone || !audioDone )
    {
        bool progress = false;
        if ( !videoDone && input.readyForMoreMediaData )
        {
            if ( ![input appendSampleBuffer:clip[vi].buf] ) { ok = false; failWhere = "video"; break; }
            if ( ++vi >= clip.size() ) { [input markAsFinished]; videoDone = true; }
            progress = true;
        }
        if ( !audioDone && audioInput && audioInput.readyForMoreMediaData )
        {
            if ( ![audioInput appendSampleBuffer:audioSamples[ai]] ) { ok = false; failWhere = "audio"; break; }
            if ( ++ai >= audioSamples.size() ) { [audioInput markAsFinished]; audioDone = true; }
            progress = true;
        }
        if ( !progress )
        {
            if ( ++spins > 20000 ) { ok = false; failWhere = "stall"; break; }   // ~10 s safety net
            usleep( 500 );
        }
        else
            spins = 0;
    }

    if ( ok )
    {
        if ( !videoDone ) [input markAsFinished];
        if ( audioInput && !audioDone ) [audioInput markAsFinished];
    }
    for ( CMSampleBufferRef as : audioSamples ) CFRelease( as );

    if ( !ok )
    {
        {
            std::ostringstream s;
            s << "clip: " << ( failWhere ? failWhere : "?" ) << " append failed (video "
              << vi << "/" << clip.size() << ", audio " << ai << "/" << audioSamples.size()
              << ") err=" << ( writer.error ? writer.error.localizedDescription.UTF8String : "none" );
            sr_RecorderBreadcrumb( s.str().c_str() );
        }
        @try { [writer cancelWriting]; } @catch ( NSException *e ) {}
        return false;
    }

    // local completion flag, so a clip and a take can finish independently
    __block bool done = false;
    [writer finishWritingWithCompletionHandler:^{ done = true; }];
    for ( int i = 0; i < 20000 && !done; ++i )
        usleep( 1000 );

    if ( !done || writer.status != AVAssetWriterStatusCompleted )
    {
        sr_RecorderBreadcrumb( "clip: finishing failed" );
        return false;
    }
    return true;
}

//! serial queue the clip muxing runs on
static dispatch_queue_t sr_ClipQueue()
{
    static dispatch_queue_t queue = NULL;
    if ( !queue )
        queue = dispatch_queue_create( "com.armagetronad.arcar.clip", DISPATCH_QUEUE_SERIAL );
    return queue;
}

bool sr_RecorderPlatformClip( double seconds, tString const & path )
{
    @autoreleasepool
    {
        if ( sr_mode != SR_BUFFER || !sr_session )
            return false;

        // flush every frame captured so far into the ring
        VTCompressionSessionCompleteFrames( sr_session, kCMTimeInvalid );

        // snapshot the window under the lock, retaining the samples we keep
        std::vector<sr_BufSample> clip;
        {
            std::lock_guard<std::mutex> lock( sr_bufferMutex );
            if ( sr_buffer.empty() )
                return false;

            double const newest = sr_buffer.back().pts;
            double const wantStart = newest - seconds;

            size_t start = 0;
            bool found = false;
            // the newest keyframe at or before the requested start
            for ( size_t i = 0; i < sr_buffer.size(); ++i )
            {
                if ( sr_buffer[i].keyframe && sr_buffer[i].pts <= wantStart )
                {
                    start = i;
                    found = true;
                }
            }
            // no keyframe that early: fall back to the oldest one we have
            if ( !found )
            {
                for ( size_t i = 0; i < sr_buffer.size(); ++i )
                {
                    if ( sr_buffer[i].keyframe )
                    {
                        start = i;
                        found = true;
                        break;
                    }
                }
            }
            if ( !found )
                start = 0;

            for ( size_t i = start; i < sr_buffer.size(); ++i )
            {
                CFRetain( sr_buffer[i].buf );
                clip.push_back( sr_buffer[i] );
            }
        }
        if ( clip.empty() )
            return false;

        double const sessionStart = clip[0].pts;
        double const endTime = clip.back().pts + 1.0 / sr_bufferFPS;
        int const freq = sr_audioFrequency;
        int const channels = sr_audioChannels;

        // copy only the audio that overlaps the clip, not the whole buffer
        std::vector<sr_AudioChunk> audio;
        {
            int const frameBytes = channels * 2;
            for ( sr_AudioChunk const &c : sr_audioChunks )
            {
                double const dur = ( freq > 0 && frameBytes > 0 )
                                 ? (double)c.data.size() / ( (double)freq * frameBytes ) : 0.0;
                if ( c.seconds + dur <= sessionStart || c.seconds >= endTime )
                    continue;
                audio.push_back( c );
            }
        }

        NSString *file = [[NSString stringWithUTF8String:path.c_str()] retain];
        std::vector<sr_BufSample> samples = std::move( clip );

        dispatch_async( sr_ClipQueue(), ^{
            @autoreleasepool
            {
                bool ok = sr_MuxClip( samples, audio, sessionStart, endTime,
                                      freq, channels, file );

                long long bytes = 0;
                NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:file error:nil];
                if ( attrs )
                    bytes = [attrs[NSFileSize] longLongValue];

                rRecorder::OnClipSaved( ok && bytes > 0,
                                        tString( file.UTF8String ? file.UTF8String : "" ),
                                        bytes );

                for ( sr_BufSample const &s : samples ) CFRelease( s.buf );
                [file release];
            }
        } );

        return true;
    }
}

void sr_RecorderPlatformBufferEnd()
{
    @autoreleasepool
    {
        if ( sr_session )
        {
            VTCompressionSessionCompleteFrames( sr_session, kCMTimeInvalid );
            VTCompressionSessionInvalidate( sr_session );
            CFRelease( sr_session );
            sr_session = NULL;
        }
        {
            std::lock_guard<std::mutex> lock( sr_bufferMutex );
            for ( sr_BufSample &s : sr_buffer ) CFRelease( s.buf );
            sr_buffer.clear();
            sr_bufferBytes = 0;
        }
        if ( sr_bufferPool )
        {
            CVPixelBufferPoolRelease( sr_bufferPool );
            sr_bufferPool = NULL;
        }

        sr_audioChunks.clear();
        sr_audioChunkBytes = 0;
        sr_audioFrequency = 0;
        sr_audioChannels = 0;

        sr_DeleteGLObjects();
        if ( sr_mode == SR_BUFFER )
            sr_mode = SR_NONE;
    }
}

// ---------------------------------------------------------------------------
// Shared per-frame capture
// ---------------------------------------------------------------------------
bool sr_RecorderPlatformFrame( double seconds )
{
    if ( sr_mode == SR_NONE )
        return false;

    // While the window is not on screen there is nothing worth capturing. Skipping
    // keeps the readback ring and the encoder from going stale, so the framerate
    // is back to normal the moment the window returns.
    if ( sr_CaptureIdle() )
    {
        if ( sr_pendingCount > 0 )
            sr_ResetCapture();
        return false;
    }

    if ( sr_mode == SR_TAKE )
    {
        if ( sr_failed || !sr_writer || !sr_input || !sr_adaptor )
            return false;
        if ( sr_writer.status != AVAssetWriterStatusWriting )
        {
            sr_failed = true;
            return false;
        }
        // If the encoder is behind we skip the capture entirely; the pending
        // frame simply waits, so the ring stays consistent.
        if ( !sr_input.readyForMoreMediaData )
        {
            ++sr_dropped;
            return false;
        }
    }
    else if ( sr_mode == SR_BUFFER )
    {
        if ( sr_failed || !sr_session )
            return false;
    }
    else
    {
        return false;
    }

    @autoreleasepool
    {
        GLint previousFbo = 0;
        glGetIntegerv( GL_FRAMEBUFFER_BINDING_EXT, &previousFbo );
        GLint previousViewport[4];
        glGetIntegerv( GL_VIEWPORT, previousViewport );

        // copy through our own texture, then scale down and read back from there
        // (see the readback ring above)
        glBindTexture( GL_TEXTURE_2D, sr_captureTexture );
        glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sr_width, sr_height );
        if ( sr_mipLevels > 0 )
            glGenerateMipmap( GL_TEXTURE_2D );
        glBindTexture( GL_TEXTURE_2D, 0 );

        glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, sr_captureFbo );
        glViewport( 0, 0, sr_outW, sr_outH );

        glPushAttrib( GL_ALL_ATTRIB_BITS );
        glDisable( GL_DEPTH_TEST );
        glDisable( GL_BLEND );
        glDisable( GL_CULL_FACE );
        glDisable( GL_LIGHTING );
        glDisable( GL_SCISSOR_TEST );
        glDisable( GL_ALPHA_TEST );
        glEnable( GL_TEXTURE_2D );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glMatrixMode( GL_PROJECTION );
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode( GL_MODELVIEW );
        glPushMatrix();
        glLoadIdentity();
        glColor4f( 1, 1, 1, 1 );
        glBindTexture( GL_TEXTURE_2D, sr_captureTexture );
        glBegin( GL_QUADS );
        glTexCoord2f( 0, 0 ); glVertex2f( -1, -1 );
        glTexCoord2f( 1, 0 ); glVertex2f(  1, -1 );
        glTexCoord2f( 1, 1 ); glVertex2f(  1,  1 );
        glTexCoord2f( 0, 1 ); glVertex2f( -1,  1 );
        glEnd();
        glBindTexture( GL_TEXTURE_2D, 0 );
        glMatrixMode( GL_PROJECTION );
        glPopMatrix();
        glMatrixMode( GL_MODELVIEW );
        glPopMatrix();
        glPopAttrib();

        int const slot = sr_issued % sr_ringSize;
        glBindBuffer( GL_PIXEL_PACK_BUFFER, sr_pbo[slot] );
        glReadPixels( 0, 0, sr_outW, sr_outH, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0 );
        glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
        glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, previousFbo );
        glViewport( previousViewport[0], previousViewport[1],
                    previousViewport[2], previousViewport[3] );

        // write out the oldest queued frame that has had its slack
        while ( sr_pendingCount - sr_pendingConsumed > sr_ringLatency )
        {
            int const i = sr_pendingConsumed % sr_ringSize;
            sr_RecorderConsume( sr_pendingSlot[i], sr_pendingTime[i] );
            ++sr_pendingConsumed;
        }

        // and queue this one for a later interval
        sr_pendingSlot[sr_pendingCount % sr_ringSize] = slot;
        sr_pendingTime[sr_pendingCount % sr_ringSize] = seconds;
        ++sr_pendingCount;
        ++sr_issued;
        return true;
    }
}

bool sr_RecorderPlatformIsHealthy()
{
    if ( sr_failed )
        return false;
    if ( sr_mode == SR_TAKE )
        return sr_writer != nil && sr_input != nil;
    if ( sr_mode == SR_BUFFER )
        return sr_session != NULL;
    return false;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

//! wraps interleaved signed 16 bit PCM in a CMSampleBuffer
static CMSampleBufferRef sr_CreateAudioSample( const unsigned char * data, int bytes,
                                               double seconds, int freq, int channels )
{
    AudioStreamBasicDescription description = {};
    description.mSampleRate       = freq;
    description.mFormatID         = kAudioFormatLinearPCM;
    description.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    description.mBytesPerPacket   = 2 * channels;
    description.mFramesPerPacket  = 1;
    description.mBytesPerFrame    = 2 * channels;
    description.mChannelsPerFrame = channels;
    description.mBitsPerChannel   = 16;

    CMFormatDescriptionRef format = NULL;
    if ( CMAudioFormatDescriptionCreate( NULL, &description, 0, NULL, 0, NULL, NULL, &format ) != noErr )
        return NULL;

    CMBlockBufferRef block = NULL;
    if ( CMBlockBufferCreateWithMemoryBlock( NULL, NULL, bytes, kCFAllocatorDefault, NULL,
                                             0, bytes, 0, &block ) != kCMBlockBufferNoErr )
    {
        CFRelease( format );
        return NULL;
    }
    CMBlockBufferReplaceDataBytes( data, block, 0, bytes );

    CMSampleTimingInfo timing;
    timing.duration              = CMTimeMake( 1, freq );
    timing.presentationTimeStamp = CMTimeMakeWithSeconds( seconds, 600 );
    timing.decodeTimeStamp       = kCMTimeInvalid;

    CMSampleBufferRef sample = NULL;
    if ( CMSampleBufferCreate( NULL, block, true, NULL, NULL, format,
                               bytes / ( 2 * channels ), 1, &timing, 0, NULL,
                               &sample ) != noErr )
        sample = NULL;

    CFRelease( block );
    CFRelease( format );
    return sample;
}

bool sr_RecorderPlatformAudio( const unsigned char * data, int bytes, double seconds )
{
    if ( bytes <= 0 )
        return false;

    // instant replay: keep the PCM for the length of the buffer; it is encoded
    // only when a clip is saved
    if ( sr_mode == SR_BUFFER )
    {
        if ( sr_audioFrequency <= 0 || sr_audioChannels <= 0 )
            return false;

        sr_AudioChunk chunk;
        chunk.data.assign( data, data + bytes );
        chunk.seconds = seconds;
        sr_audioChunkBytes += (size_t)bytes;
        sr_audioChunks.push_back( std::move( chunk ) );

        double const newest = sr_audioChunks.back().seconds;
        double const cutoff = newest - sr_bufferSeconds - 2.0;
        while ( sr_audioChunks.size() > 1 && sr_audioChunks[1].seconds < cutoff )
        {
            sr_audioChunkBytes -= sr_audioChunks.front().data.size();
            sr_audioChunks.pop_front();
        }
        while ( sr_audioChunkBytes > sr_audioMaxBytes && sr_audioChunks.size() > 1 )
        {
            sr_audioChunkBytes -= sr_audioChunks.front().data.size();
            sr_audioChunks.pop_front();
        }
        return true;
    }

    // take recording: append straight to the writer's audio input
    if ( sr_mode != SR_TAKE || !sr_audioInput )
        return false;
    if ( !sr_writer || sr_writer.status != AVAssetWriterStatusWriting )
        return false;

    // if the writer is busy, keep the samples: they are offered again next frame
    if ( !sr_audioInput.readyForMoreMediaData )
        return false;

    bool written = false;
    @autoreleasepool
    {
        CMSampleBufferRef sample = sr_CreateAudioSample( data, bytes, seconds,
                                                         sr_audioFrequency, sr_audioChannels );
        if ( sample )
        {
            written = [sr_audioInput appendSampleBuffer:sample];
            CFRelease( sample );
        }
    }
    return written;
}

bool sr_RecorderPlatformHasAudio()
{
    if ( sr_mode == SR_TAKE )
        return sr_audioInput != nil;
    if ( sr_mode == SR_BUFFER )
        return sr_audioFrequency > 0 && sr_audioChannels > 0;
    return false;
}

double sr_RecorderPlatformEnd()
{
    double seconds = 0;

    @autoreleasepool
    {
        if ( !sr_writer )
        {
            sr_mode = SR_NONE;
            return 0;
        }

        // don't lose the frames still in flight
        while ( sr_pendingConsumed < sr_pendingCount )
        {
            int const i = sr_pendingConsumed % sr_ringSize;
            sr_RecorderConsume( sr_pendingSlot[i], sr_pendingTime[i] );
            ++sr_pendingConsumed;
        }

        sr_DeleteGLObjects();

        AVAssetWriter *writer = sr_writer;
        AVAssetWriterInput *vinput = sr_input;
        AVAssetWriterInput *ainput = sr_audioInput;
        AVAssetWriterInputPixelBufferAdaptor *adaptor = sr_adaptor;
        long const dropped = sr_dropped;
        double const lastStamp = sr_lastStamp;

        // clear the globals first so a new take can start even if this finish is
        // slow; ownership of the objects stays here until they are released
        sr_writer = nil;
        sr_input = nil;
        sr_audioInput = nil;
        sr_adaptor = nil;
        sr_audioFrequency = 0;
        sr_audioChannels = 0;
        if ( sr_mode == SR_TAKE )
            sr_mode = SR_NONE;

        // Finalise, but never let AVFoundation throw into the game loop and never
        // free the objects while the writer may still be using them.
        @try
        {
            if ( writer.status == AVAssetWriterStatusWriting )
            {
                if ( vinput ) [vinput markAsFinished];
                if ( ainput ) [ainput markAsFinished];

                __block bool done = false;
                [writer finishWritingWithCompletionHandler:^{ done = true; }];

                // wait, but not so long that the game looks hung
                for ( int i = 0; i < 2000 && !done; ++i )
                    usleep( 1000 );
            }
            else
            {
                [writer cancelWriting];
            }
        }
        @catch ( NSException *e )
        {
            std::ostringstream s;
            s << "finalising raised " << ( e.name ? e.name.UTF8String : "?" )
              << ": " << ( e.reason ? e.reason.UTF8String : "?" );
            sr_RecorderBreadcrumb( s.str().c_str() );
            @try { [writer cancelWriting]; } @catch ( NSException *e2 ) {}
        }

        if ( writer.status == AVAssetWriterStatusWriting )
        {
            // Still finishing. Do not release anything: the writer is alive and
            // will finish on its own. Leaking once beats a use-after-free.
            sr_RecorderBreadcrumb( "finalising in the background; not releasing writer yet" );
        }
        else
        {
            if ( writer.status != AVAssetWriterStatusCompleted )
                con << "Recording: finishing failed: "
                    << ( writer.error ? writer.error.localizedDescription.UTF8String : "unknown" ) << "\n";
            else if ( dropped > 0 )
                con << "Recording: " << dropped << " frames dropped (encoder fell behind)\n";

            [vinput release];
            if ( ainput ) [ainput release];
            [adaptor release];
            [writer release];
        }

        int const fps = rRecorder::sr_movieFPS > 0 ? rRecorder::sr_movieFPS : 60;
        seconds = lastStamp > 0 ? lastStamp + 1.0 / fps : 0;
    }

    return seconds;
}
