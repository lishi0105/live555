/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2024 Live Networks, Inc.  All rights reserved.
// A file source that is a plain byte stream (rather than frames)
// C++ header

#ifndef _BYTE_STEAM_FRAME_SOURCE_HH
#define _BYTE_STEAM_FRAME_SOURCE_HH

#include <stdint.h>

#include <mutex>
#include "StreamServerMediaSubsession.hh"

#include "FramedSource.hh"

class ByteStreamFrameSource : public FramedSource {
public:
    static ByteStreamFrameSource *createNew(UsageEnvironment &env,
                                            unsigned fps = 60);
    Boolean doPutFrames(const unsigned char *buffer, unsigned bufferSize, Boolean ifKey) ;
    Boolean doPutIDRFrames(const unsigned char *buffer, unsigned bufferSize) ;
    void setFrameSourceChanged(onFrameSourceStateChanged cb_){
        fcb = cb_;
        if(fcb){
            fcb(this, True);
        }
    }

protected:
    ByteStreamFrameSource(UsageEnvironment &env, unsigned fps);
    virtual ~ByteStreamFrameSource();
    void doGetNextFrame() override;

private:
    bool getIDRFrame();
    void doReadFromMemory();
    void doClosure();

private:
    std::mutex m_mutex;
    std::mutex m_IDRMutex;
    unsigned ffps;
    unsigned char *fIDRFrame;
    unsigned fIDRFrameSize;
    unsigned char *fBuffer;
    unsigned fBufferSize;
    unsigned char *fTruncateBuffer;
    unsigned fNumTruncatedBytes_;
    Boolean fClosed;
    unsigned fNoFrameCount;
    Boolean fHaveStartedReading;
    onFrameSourceStateChanged fcb;
};
#endif  // !_BYTE_STEAM_FRAME_SOURCE_HH