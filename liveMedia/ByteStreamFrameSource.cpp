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
// A class for streaming data from a (static) memory buffer, as if it were a
// file. Implementation

#include "ByteStreamFrameSource.hh"
#include "LiveLogger.hh"
#include "LiveCommon.hh"
#include <sys/time.h>

ByteStreamFrameSource::ByteStreamFrameSource(UsageEnvironment &env, unsigned fps)
    : FramedSource(env),
      fIDRFrame(NULL),
      fIDRFrameSize(0),
      fBuffer(NULL),
      fBufferSize(0),
      fNoFrameCount(0),
      fNumTruncatedBytes_(0),
      fHaveStartedReading(False),
      fClosed(False)
{
    ffps = fps;
    if(fps <= 0)ffps = 60;
    fTruncateBuffer = new unsigned char[150000];
    if(fTruncateBuffer == NULL){
        LIVE_LOG(ERROR)<<"new fTruncateBuffer failed";
        throw std::runtime_error("fTruncateBuffer malloc error");
    }
    fcb = NULL;
    LIVE_LOG(WARN)<< "ByteStreamFrameSource::ByteStreamFrameSource()";
}

ByteStreamFrameSource::~ByteStreamFrameSource() {
    if(fcb){
        fcb(this, False);
    }
    if(fTruncateBuffer != NULL){
        delete[] fTruncateBuffer;
        fTruncateBuffer = NULL;
    }
    if(fIDRFrame != NULL){
        delete[] fIDRFrame;
        fIDRFrame = NULL;
    }
    if(fBuffer != NULL){
        delete[] fBuffer;
        fBuffer = NULL;
    }
    fClosed = True;
    
    LIVE_LOG(TRACE)<< "ByteStreamFrameSource::~ByteStreamFrameSource()";
}

ByteStreamFrameSource *ByteStreamFrameSource::createNew(UsageEnvironment &env,
                                                        unsigned fps) {
    return new ByteStreamFrameSource(env, fps);
}

void ByteStreamFrameSource::doGetNextFrame() {
    fDurationInMicroseconds = 1000000 / ffps;
    if(!fHaveStartedReading){
        if(getIDRFrame()){
            fHaveStartedReading = True;
        }
        nextTask() = envir().taskScheduler().scheduleDelayedTask(
            fDurationInMicroseconds, (TaskFunc *)FramedSource::afterGetting, this);
        return;
    }
    doReadFromMemory();
    // nextTask() = envir().taskScheduler().scheduleDelayedTask(
    //     fDurationInMicroseconds, (TaskFunc *)FramedSource::afterGetting, this);
    FramedSource::afterGetting(this);
}

bool ByteStreamFrameSource::getIDRFrame() {
    std::unique_lock<std::mutex> lock(m_IDRMutex);
    int timeout = 0;
    
    while((fIDRFrame == NULL || fIDRFrameSize == 0) && timeout < 100) {
        timeout++;
        usleep(1000);
    }
    if(fIDRFrame == NULL || fIDRFrameSize == 0){
        return false;
    }
    uint32_t newFrameSize = fIDRFrameSize > fMaxSize ? fMaxSize : fIDRFrameSize;
    memmove(fTo, fIDRFrame, newFrameSize);
    fFrameSize = newFrameSize;
    gettimeofday(&fPresentationTime, NULL);
    fDurationInMicroseconds = 0;
    return true;
}

void ByteStreamFrameSource::doClosure() 
{
    if(fIDRFrame != NULL){
        delete[] fIDRFrame;
        fIDRFrame = NULL;
    }
    if(fBuffer != NULL){
        delete[] fBuffer;
        fBuffer = NULL;
    }
    fIDRFrameSize = 0;
    fBufferSize = 0;
    fNumTruncatedBytes_ = 0;
    fHaveStartedReading = False;
    fNoFrameCount = 0;
    fClosed = True;
    handleClosure();
}

void ByteStreamFrameSource::doReadFromMemory() 
{
    int timeout = 0;
    while (timeout < 100) {
        timeout++;
        if(fBuffer == NULL || fBufferSize == 0) {
            usleep(1000);
            continue;
        }
        break;
    } 
    m_mutex.lock();
    if (fBuffer == NULL || fBufferSize == 0) {
        m_mutex.unlock();
        fFrameSize = 0;
        fDurationInMicroseconds = 0;
        LIVE_LOG(INFO)<<"fBuffer == NULL || fBufferSize == 0";
        if(fNoFrameCount++ > 3){
            LIVE_LOG(WARN)<< "too long time no frame, close the connection";
            doClosure();
            return;
        }
        // nextTask() = envir().taskScheduler().scheduleDelayedTask(
        //     fDurationInMicroseconds, (TaskFunc *)FramedSource::afterGetting, this);
        afterGetting(this);
        return;
    }
    m_mutex.unlock();
    fNoFrameCount = 0;
    uint32_t fMaxSize_ = fMaxSize - fNumTruncatedBytes_;
    uint32_t newFrameSize = fBufferSize > fMaxSize_ ? fMaxSize_ : fBufferSize;
    // LIVE_LOG(INFO)<<"fMaxSize: " <<fMaxSize 
    //                 <<" newSize : "<< newFrameSize 
    //                 <<" Size : "<<fBufferSize
    //                 <<" Truncated : "<<fNumTruncatedBytes_
    //                 <<" fDuration : "<<fDurationInMicroseconds;
    if(fNumTruncatedBytes_ > 0){
        memcpy(fTo, fTruncateBuffer, fNumTruncatedBytes_);
        fFrameSize = fNumTruncatedBytes_;
    }else{
        fFrameSize = 0;
    }
    memcpy(fTo+fNumTruncatedBytes_, fBuffer, newFrameSize);
    fFrameSize += newFrameSize;
    if(newFrameSize < fBufferSize){
        fNumTruncatedBytes_ = fBufferSize - newFrameSize;
        memcpy(fTruncateBuffer, fBuffer + newFrameSize, fNumTruncatedBytes_);
    }else{
        fNumTruncatedBytes_ = 0;
    }
    gettimeofday(&fPresentationTime, NULL);
    // fDurationInMicroseconds = 0;
    delete[] fBuffer;
    fBuffer = NULL;
    m_mutex.unlock();
}

Boolean ByteStreamFrameSource::doPutIDRFrames(const unsigned char *buffer,unsigned bufferSize) 
{
    std::unique_lock<std::mutex> lock(m_IDRMutex);
    if(fIDRFrameSize > 0 || fClosed == True) return False;
    if (buffer == NULL || bufferSize == 0) {
        return False;
    }
    if(fIDRFrame != NULL){
        delete[] fIDRFrame;
        fIDRFrame = NULL;
    }
    fIDRFrame = new unsigned char[bufferSize + 1];
    if(fIDRFrame == NULL){
        LIVE_LOG(ERROR)<<"new fIDRFrame failed";
        throw std::runtime_error("fIDRFrame malloc error");
    }
    LIVE_LOG(TRACE)<< "setIDRFrame size:" << bufferSize;
    memcpy(fIDRFrame, buffer, bufferSize);
    fIDRFrame[bufferSize] = 0x0;
    fIDRFrameSize = bufferSize;
    return True;
}

Boolean 
ByteStreamFrameSource::doPutFrames(const uint8_t *buffer,unsigned bufferSize, Boolean ifKey) 
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (buffer == NULL || bufferSize == 0 || fClosed == True) {
        return false;
    }
    if (fBuffer != NULL) {
        delete[] fBuffer;
        fBuffer = NULL;
    }
    (void)ifKey;
    fBuffer = new uint8_t[bufferSize + 1];
    memcpy(fBuffer, buffer, bufferSize);
    fBuffer[bufferSize] = 0x0;
    fBufferSize = bufferSize;
    return true;
}
