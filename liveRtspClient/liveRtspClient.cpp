/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-17 14:19:17
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-15 09:07:49
 */
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
// Copyright (c) 1996-2023, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can
// potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only
// to illustrate how to develop your own RTSP client application.  For a
// full-featured RTSP client application - with much more functionality, and
// many options - see "openRTSP": http://www.live555.com/openRTSP/

#include "liveRtspClient.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "LiveCommon.hh"
#include "LiveLogger.hh"
#include "LiveUtils.hh"
#include "LiveVersion.hh"

static uint32_t startcode = 0x01000000;

bool LiveRtspClient::bRecvDebug = false;

// Forward function definitions:

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode,
                           char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode,
                        char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode,
                       char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(
    void* clientData);  // called when a stream's subsession (e.g., audio or
                        // video substream) ends
void subsessionByeHandler(void* clientData, char const* reason);
// called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
// called at the end of a stream's expected duration (if the stream has not
// already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL,
             liveRtspDataCallBack pCallback, bool bRtpOverTcp);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging
// output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env,
                             const RTSPClient& rtspClient) {
    return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for
// debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env,
                             const MediaSubsession& subsession) {
    return env << subsession.mediumName() << "/" << subsession.codecName();
}

// Define a class to hold per-stream state that we maintain throughout each
// stream's lifetime:

class StreamClientState {
public:
    StreamClientState();
    virtual ~StreamClientState();

public:
    MediaSubsessionIterator* iter;
    MediaSession* session;
    MediaSubsession* subsession;
    TaskToken streamTimerTask;
    double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL,
// once), then you can define and use just a single "StreamClientState"
// structure, as a global variable in your application.  However, because - in
// this demo application - we're showing how to play multiple streams,
// concurrently, we can't do that.  Instead, we have to have a separate
// "StreamClientState" structure for each "RTSPClient".  To do this, we subclass
// "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient : public RTSPClient {
public:
    static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
                                    int verbosityLevel = 0,
                                    char const* applicationName = NULL,
                                    portNumBits tunnelOverHTTPPortNum = 0,
                                    Boolean fUsetTcpOverRtp = False);
    void setDataCallback(liveRtspDataCallBack p) {
        pDataCallback = p;
        if (pDataCallback == nullptr) {
            LIVE_LOG(ERROR) << url_ << " pDataCallback is nullptr";
        }
    }

protected:
    ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel, char const* applicationName,
                  portNumBits tunnelOverHTTPPortNum, Boolean fUsetTcpOverRtp);
    // called only by createNew();
    virtual ~ourRTSPClient();

public:
    liveRtspDataCallBack pDataCallback;
    StreamClientState scs;
    std::string url_;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each
// subsession (i.e., each audio or video 'substream'). In practice, this might
// be a class (or a chain of classes) that decodes and then renders the incoming
// audio or video. Or it might be a "FileSink", for outputting the received data
// into a file (as is done by the "openRTSP" application). In this example code,
// however, we define a simple 'dummy' sink that receives incoming data, but
// does nothing with it.

class DummySink : public MediaSink {
public:
    static DummySink* createNew(
        UsageEnvironment& env,
        MediaSubsession&
            subsession,  // identifies the kind of data that's being received
        char const* streamId =
            NULL);  // identifies the stream itself (optional)
    void setDataCallback(liveRtspDataCallBack p);
    void setRtspUrl(std::string url);

private:
    DummySink(UsageEnvironment& env, MediaSubsession& subsession,
              char const* streamId);
    // called only by "createNew()"
    virtual ~DummySink();

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds);
    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime,
                           unsigned durationInMicroseconds);

    Boolean handleVideoFrame(const char*, const unsigned frameSize,
                             const unsigned numTruncatedBytes,
                             struct timeval presentationTime);

    Boolean handleAudioFrame(const char*, const unsigned frameSize,
                             const unsigned numTruncatedBytes,
                             struct timeval presentationTime);

private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();
    liveRtspDataCallBack pDataCallback;

private:
    std::string url_;
    uint32_t offset;
    u_int8_t* fReceiveBuffer;
    MediaSubsession& fSubsession;
    char* fStreamId;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL \
    1  // by default, print verbose output from each "RTSPClient"

void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL,
             liveRtspDataCallBack pCallback, bool bRtpOverTcp) {
    // Begin by creating a "RTSPClient" object.  Note that there is a separate
    // "RTSPClient" object for each stream that we wish to receive (even if more
    // than stream uses the same "rtsp://" URL).
    Boolean bUseTcpOverRtp = False;
    bRtpOverTcp == true ? bUseTcpOverRtp = True : bUseTcpOverRtp = False;
    std::string url = rtspURL;
    std::string username = "";
    std::string password = "";
    std::string UnauthorizedPath = "";
    if (!LiveUtils::parsingRTSPURL(url, username, password, UnauthorizedPath)) {
        env << "URL \"" << rtspURL << "\": not authorized \n";
        RTSPClient* rtspClient =
            ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL,
                                     progName, 0, bUseTcpOverRtp);
        if (rtspClient == NULL) {
            env << "Failed to create a RTSP client for URL \"" << rtspURL
                << "\": " << env.getResultMsg() << "\n";
            return;
        }
        rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
        ourRTSPClient* pRtsp = (ourRTSPClient*)rtspClient;
        pRtsp->setDataCallback(pCallback);
        return;
    }
    const char* unAuthenticator_url = UnauthorizedPath.c_str();
    env << "URL \"" << rtspURL << "\": authorized : [" << username.c_str()
        << " / " << password.c_str() << "]: " << unAuthenticator_url << "\n";
    RTSPClient* rtspClient = ourRTSPClient::createNew(
        env, unAuthenticator_url, RTSP_CLIENT_VERBOSITY_LEVEL, progName, 0,
        bUseTcpOverRtp);

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the
    // stream. Note that this command - like all RTSP commands - is sent
    // asynchronously; we do not block, waiting for a response. Instead, the
    // following function call returns immediately, and we handle the RTSP
    // response later, from within the event loop:
    Authenticator authenticator(username.c_str(), password.c_str());
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE, &authenticator);
    ourRTSPClient* pRtsp = (ourRTSPClient*)rtspClient;
    pRtsp->setDataCallback(pCallback);
}

// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode,
                           char* resultString) {
    do {
        UsageEnvironment& env = rtspClient->envir();                 // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

        if (resultCode != 0) {
            env << *rtspClient
                << "Failed to get a SDP description: " << resultString << "\n";
            delete[] resultString;
            break;
        }

        char* const sdpDescription = resultString;
        env << *rtspClient << "Got a SDP description:\n"
            << sdpDescription << "\n";

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription;  // because we don't need it anymore
        if (scs.session == NULL) {
            env << *rtspClient
                << "Failed to create a MediaSession object from the SDP "
                   "description: "
                << env.getResultMsg() << "\n";
            break;
        } else if (!scs.session->hasSubsessions()) {
            env << *rtspClient
                << "This session has no media subsessions (i.e., no \"m=\" "
                   "lines)\n";
            break;
        }

        // Then, create and set up our data source objects for the session.  We
        // do this by iterating over the session's 'subsessions', calling
        // "MediaSubsession::initiate()", and then sending a RTSP "SETUP"
        // command, on each one. (Each 'subsession' will have its own data
        // source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP,
// change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void setupNextSubsession(RTSPClient* rtspClient) {
    UsageEnvironment& env = rtspClient->envir();                 // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            env << *rtspClient << "Failed to initiate the \"" << *scs.subsession
                << "\" subsession: " << env.getResultMsg() << "\n";
            setupNextSubsession(
                rtspClient);  // give up on this subsession; go to the next one
        } else {
            env << *rtspClient << "Initiated the \"" << *scs.subsession
                << "\" subsession (";
            if (scs.subsession->rtcpIsMuxed()) {
                env << "client port " << scs.subsession->clientPortNum();
            } else {
                env << "client ports " << scs.subsession->clientPortNum() << "-"
                    << scs.subsession->clientPortNum() + 1;
            }
            env << ")\n";

            // Continue setting up this subsession, by sending a RTSP "SETUP"
            // command:
            rtspClient->sendSetupCommand_v1(*scs.subsession, continueAfterSETUP,
                                            False);
            // rtspClient->sendSetupCommand(*scs.subsession,
            // continueAfterSETUP,False, REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }

    // We've finished setting up all of the subsessions.  Now, send a RTSP
    // "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an
        // appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY,
                                    scs.session->absStartTime(),
                                    scs.session->absEndTime());
    } else {
        scs.duration =
            scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode,
                        char* resultString) {
    do {
        UsageEnvironment& env = rtspClient->envir();                 // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to set up the \"" << *scs.subsession
                << "\" subsession: " << resultString << "\n";
            break;
        }

        env << *rtspClient << "Set up the \"" << *scs.subsession
            << "\" subsession (";
        if (scs.subsession->rtcpIsMuxed()) {
            env << "client port " << scs.subsession->clientPortNum();
        } else {
            env << "client ports " << scs.subsession->clientPortNum() << "-"
                << scs.subsession->clientPortNum() + 1;
        }
        env << ")\n";

        // Having successfully setup the subsession, create a data sink for it,
        // and call "startPlaying()" on it. (This will prepare the data sink to
        // receive data; the actual flow of data from the client won't start
        // happening until later, after we've sent a RTSP "PLAY" command.)
        scs.subsession->sink =
            DummySink::createNew(env, *scs.subsession, rtspClient->url());
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            env << *rtspClient << "Failed to create a data sink for the \""
                << *scs.subsession << "\" subsession: " << env.getResultMsg()
                << "\n";
            break;
        }

        env << *rtspClient << "Created a data sink for the \""
            << *scs.subsession << "\" subsession\n";
        scs.subsession->miscPtr =
            rtspClient;  // a hack to let subsession handler functions get the
                         // "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                           subsessionAfterPlaying,
                                           scs.subsession);
        // Also set a handler to be called if a RTCP "BYE" arrives for this
        // subsession:
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeWithReasonHandler(
                subsessionByeHandler, scs.subsession);
        }
        DummySink* pSink = (DummySink*)(scs.subsession->sink);
        pSink->setDataCallback(((ourRTSPClient*)rtspClient)->pDataCallback);
        pSink->setRtspUrl(((ourRTSPClient*)rtspClient)->url_);
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode,
                       char* resultString) {
    Boolean success = False;

    do {
        UsageEnvironment& env = rtspClient->envir();                 // alias
        StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

        if (resultCode != 0) {
            env << *rtspClient
                << "Failed to start playing session: " << resultString << "\n";
            break;
        }

        // Set a timer to be handled at the end of the stream's expected
        // duration (if the stream does not already signal its end using a RTCP
        // "BYE").  This is optional.  If, instead, you want to keep the stream
        // active - e.g., so you can later 'seek' back within it and do another
        // RTSP "PLAY" - then you can omit this code. (Alternatively, if you
        // don't want to receive the entire stream, you could set this timer for
        // some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop =
                2;  // number of seconds extra to delay, after the stream's
                    // expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
                uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
        }

        env << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
            env << " (for up to " << scs.duration << " seconds)";
        }
        env << "...\n";

        success = True;
    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}

// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL)
            return;  // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData, char const* reason) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    UsageEnvironment& env = rtspClient->envir();  // alias

    env << *rtspClient << "Received RTCP \"BYE\"";
    if (reason != NULL) {
        env << " (reason:\"" << reason << "\")";
        delete[](char*) reason;
    }
    env << " on \"" << *subsession << "\" subsession\n";

    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
    ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
    StreamClientState& scs = rtspClient->scs;  // alias

    scs.streamTimerTask = NULL;

    // Shut down the stream:
    shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
    UsageEnvironment& env = rtspClient->envir();                 // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

    // First, check whether any subsessions have still to be closed:
    if (scs.session != NULL) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) {
            if (subsession->sink != NULL) {
                Medium::close(subsession->sink);
                subsession->sink = NULL;

                if (subsession->rtcpInstance() != NULL) {
                    subsession->rtcpInstance()->setByeHandler(
                        NULL, NULL);  // in case the server sends a RTCP "BYE"
                                      // while handling "TEARDOWN"
                }

                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown
            // the stream. Don't bother handling the response to the "TEARDOWN".
            rtspClient->sendTeardownCommand(*scs.session, NULL);
        }
    }

    env << *rtspClient << "Closing the stream.\n";
    Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState"
    // structure to get reclaimed.
}

// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env,
                                        char const* rtspURL, int verbosityLevel,
                                        char const* applicationName,
                                        portNumBits tunnelOverHTTPPortNum,
                                        Boolean bUseTcpOverRtp) {
    return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName,
                             tunnelOverHTTPPortNum, bUseTcpOverRtp);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
                             int verbosityLevel, char const* applicationName,
                             portNumBits tunnelOverHTTPPortNum,
                             Boolean bUseTcpOverRtp)
    : RTSPClient(env, rtspURL, verbosityLevel, applicationName,
                 tunnelOverHTTPPortNum, -1, bUseTcpOverRtp) {
    url_ = std::string(rtspURL);
}

ourRTSPClient::~ourRTSPClient() {
    pDataCallback = nullptr;
    url_ = "";
}

// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
    : iter(NULL),
      session(NULL),
      subsession(NULL),
      streamTimerTask(NULL),
      duration(0.0) {}

StreamClientState::~StreamClientState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask"
        // (if set)
        UsageEnvironment& env = session->envir();  // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
}

// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we
// still need to receive it. Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE (1024 * 1024 * 4)

DummySink* DummySink::createNew(UsageEnvironment& env,
                                MediaSubsession& subsession,
                                char const* streamId) {
    return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession,
                     char const* streamId)
    : MediaSink(env), fSubsession(subsession) {
    offset = 0;
    url_ = "";
    pDataCallback = nullptr;
    fStreamId = strDup(streamId);
    fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE + 8];
}

DummySink::~DummySink() {
    url_ = "";
    pDataCallback = nullptr;
    delete[] fReceiveBuffer;
    delete[] fStreamId;
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
    DummySink* sink = (DummySink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime,
                            durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then
// comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME

void DummySink::afterGettingFrame(unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned /*durationInMicroseconds*/) {
    // We've just received a frame of data.  (Optionally) print out information
    // about it:
#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
    if (LiveRtspClient::bRecvDebug) {
        if (fStreamId != NULL) envir() << "Stream \"" << fStreamId << "\"; ";
        envir() << fSubsession.mediumName() << "/" << fSubsession.codecName()
                << ":\tReceived " << frameSize << " bytes";

        if (numTruncatedBytes > 0)
            envir() << " (with " << numTruncatedBytes << " bytes truncated)";

        char uSecsStr[6 + 1];  // used to output the 'microseconds' part of the
                               // presentation time
        sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
        envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec
                << "." << uSecsStr;

        if (fSubsession.rtpSource() != NULL &&
            !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
            envir() << "!";  // mark the debugging output to indicate that this
                             // presentation time is not RTCP-synchronized
        }
#ifdef DEBUG_PRINT_NPT
        envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
        envir() << "\n";
#endif
    }
    if (fSubsession.mediumName() == NULL) {
        LIVE_LOG(ERROR) << "mediumName NULL error";
    } else {
        if (strcmp(fSubsession.mediumName(), "video") == 0) {
            handleVideoFrame(fSubsession.codecName(), frameSize,
                             numTruncatedBytes, presentationTime);
        } else if (strcmp(fSubsession.mediumName(), "audio") == 0) {
            handleAudioFrame(fSubsession.codecName(), frameSize,
                             numTruncatedBytes, presentationTime);
        } else {
            LIVE_LOG(ERROR)
                << "[" << fSubsession.mediumName() << "] not video/audio error";
        }
    }
    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean DummySink::handleVideoFrame(const char* codecName, const unsigned size,
                                    const unsigned numTruncatedBytes,
                                    struct timeval presentationTime) {
    if (codecName == NULL) {
        LIVE_LOG(ERROR) << "codecName NULL error";
        return False;
    }
    if ((strcmp(codecName, "H264") != 0) && (strcmp(codecName, "H265") != 0)) {
        LIVE_LOG(ERROR) << "video codecName [" << codecName
                        << "] not support error";
        return False;
    }
    memcpy(fReceiveBuffer + offset, &startcode, 4);
    unsigned frameSize = size + 4;
    uint8_t* pRecv = fReceiveBuffer + (4 + offset);
    uint64_t pts = presentationTime.tv_sec * 1000000 + presentationTime.tv_usec;
    auto codec = LiveRtspFrame::LIVE_CODEC_ID_H264;
    auto type = LiveRtspFrame::LIVE_FRAME_TYPE_NONE;
    if (strcmp(codecName, "H264") == 0) {
        codec = LiveRtspFrame::LIVE_CODEC_ID_H264;
        avc_nalu_header nalu;
        memcpy(&nalu, pRecv, sizeof(nalu));
        // {
        // char buf[64] = {0};
        // snprintf(buf, sizeof(buf), "%x 0x%02X (%u)",pRecv[0], nalu.type,
        // nalu.type); LIVE_LOG(TRACE)<<"frame type ["<< buf<<"]";
        // }
        if (nalu.type == 0x06) {
            if(offset != 0){
                // some IDR frame contain SEI frame
                offset += frameSize;
                return True;
            }
            type = LiveRtspFrame::LIVE_FRAME_NAL_SEI;
        } else if (nalu.type == 0x05) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_I;
        } else if (nalu.type == 0x07 || nalu.type == 0x08) {
            offset += frameSize;
            return True;
        } else if (nalu.type == 0x09) {
            type = LiveRtspFrame::LIVE_FRAME_NAL_AUD;
        } else if (nalu.type == 0x12) {
            type = LiveRtspFrame::LIVE_FRAME_NAL_FILLER;
        } else if (nalu.type == 0x01) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_P;
        } else {
            char buf[128] = {0};
            snprintf(buf, sizeof(buf), "0x%02X (%u)", nalu.type, nalu.type);
            LIVE_LOG(ERROR) << "not support frame type [" << buf << "]";
            if (frameSize < 16) {
                LIVE_LOG(ERROR) << LiveUtils::uint8ToHexStr(pRecv, frameSize);
            } else {
                LIVE_LOG(ERROR) << LiveUtils::uint8ToHexStr(pRecv, 16);
            }
        }
        // printf("h264 nalu:%.2X %.2X size:%d\n",pRecv[0], nalu, frameSize);
    } else if (strcmp(codecName, "H265") == 0) {
        codec = LiveRtspFrame::LIVE_CODEC_ID_H265;
        hevc_nalu_header nalu;
        memcpy(&nalu, pRecv, sizeof(nalu));
        // {
        // char buf[64] = {0};
        // snprintf(buf, sizeof(buf), "%x 0x%02X (%u)",pRecv[0], nalu.type,
        // nalu.type); LIVE_LOG(TRACE)<<"frame type ["<< buf<<"]";
        // }
        if (nalu.type == 0x13 /*19*/ || nalu.type == 0x14 /*20*/) {  // IDR
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_I;
        } else if (nalu.type == 0x15 /*21*/) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_CRA;
        } else if (nalu.type == 0x20 /*32*/ || nalu.type == 0x21 /*33*/ ||
                   nalu.type == 0x22 /*34*/) {
            offset += frameSize;
            return True;
        } else if (nalu.type == 0x23 /*35*/) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_AUD;
        } else if (nalu.type == 0x24 /*36*/) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_EOS;
        } else if (nalu.type == 0x25 /*37*/) {
            frameSize += offset;
            offset = 0;
            type = LiveRtspFrame::LIVE_FRAME_NAL_EOB;
        } else if (nalu.type == 0x27 /*39*/ || nalu.type == 0x28 /*40*/) {
            if(offset != 0){
                // some IDR frame contain SEI frame
                offset += frameSize;
                return True;
            }
            type = LiveRtspFrame::LIVE_FRAME_NAL_SEI;
        } else {
            if (nalu.type < 0x20 /*31*/) {
                frameSize += offset;
                offset = 0;
                type = LiveRtspFrame::LIVE_FRAME_NAL_P;

            } else {
                char buf[128] = {0};
                snprintf(buf, sizeof(buf), "0x%02X (%u)", nalu.type, nalu.type);
                LIVE_LOG(ERROR) << "not support frame type [" << buf << "]";
                offset = 0;
            }
        }
    }
    if (pDataCallback != nullptr) {
        auto pFrame = LiveRtspFrame::construct(fReceiveBuffer + offset,
                                               frameSize, codec, type, pts);
        pDataCallback(url_, pFrame);
    }
    // LiveUtils::outputHexBuf(std::cout, fReceiveBuffer,
    // transByte>32?32:transByte);
    return True;
}

Boolean DummySink::handleAudioFrame(const char* codecName,
                                    const unsigned frameSize,
                                    const unsigned numTruncatedBytes,
                                    struct timeval presentationTime) {
    if (codecName == NULL) {
        LIVE_LOG(ERROR) << "codecName NULL error";
        return False;
    }
    auto codec = LiveRtspFrame::LIVE_CODEC_ID_NONE;
    auto type = LiveRtspFrame::LIVE_FRAME_TYPE_NONE;
    if (strcmp(codecName, "PCMA") == 0) {
        codec = LiveRtspFrame::LIVE_CODEC_ID_PCMA;
        type = LiveRtspFrame::LIVE_FRAME_TYPE_PCMA;
    } else if (strcmp(codecName, "PCMU") == 0) {
        codec = LiveRtspFrame::LIVE_CODEC_ID_PCMU;
        type = LiveRtspFrame::LIVE_FRAME_TYPE_PCMU;
    }
    if (pDataCallback != nullptr) {
        auto pFrame = LiveRtspFrame::construct(fReceiveBuffer + (offset + 4),
                                               frameSize, codec, type);
        pDataCallback(url_, pFrame);
    }
    // LIVE_LOG(INFO)<<"audio codecName : ["<<codecName <<"] ";
    return True;
}

void DummySink::setDataCallback(liveRtspDataCallBack p) {
    pDataCallback = p;
    if (pDataCallback == nullptr) {
        LIVE_LOG(ERROR) << "pDataCallback nullptr error";
    }
}

void DummySink::setRtspUrl(std::string rtspUrl_) { url_ = rtspUrl_; }

Boolean DummySink::continuePlaying() {
    if (fSource == NULL) return False;  // sanity check (should not happen)
    if (fReceiveBuffer == NULL) {
        LIVE_LOG(ERROR) << "fReceiveBuffer error";
        return False;
    }
    // Request the next frame of data from our input source.
    // "afterGettingFrame()" will get called later, when it arrives:
    const uint32_t maxSize = DUMMY_SINK_RECEIVE_BUFFER_SIZE;
    if (offset >= maxSize) {
        LIVE_LOG(ERROR) << "offset:" << offset << " too larget";
        offset = 0;
    }
    uint8_t* pRecv = fReceiveBuffer + (4 + offset);
    fSource->getNextFrame(pRecv, (maxSize - offset - 4), afterGettingFrame,
                          this, onSourceClosure, this);
    return True;
}

LiveRtspClient::LiveRtspClient(const std::string& rtsp_url,
                               liveRtspDataCallBack callback,
                               liveRtspServerLost serverlost,
                               bool bRtpOverTcp) {
    callback_ = callback;
    serverLost_ = serverlost;
    u32RecvFrameCount = 0;
    u32RecvDataBit = 0;
    fps_rt_ = 0;
    fps_average_ = 0.0;
    bitrate_rt_ = 0;
    bitrate_average_ = 0;
    eventLoopWatchVariable = 0;
    is_alive_ = false;
    bStart = false;
    bUseTcpOverRtp = bRtpOverTcp;
    rtsp_url_ = rtsp_url;
    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    unsigned int i = 1;
    char* c = (char*)&i;
    if (*c) {
        startcode = 0x01000000;
        printf("Little endian\n");
    } else {
        printf("Big endian\n");
    }
}

LiveRtspClient::~LiveRtspClient() {
    serverLost_ = nullptr;
    callback_ = nullptr;
    stop();
    env->reclaim();
    env = nullptr;
    delete scheduler;
    scheduler = nullptr;
}

const std::string LiveRtspClient::version() {
    std::string liveVersion =
        std::string("live") +
        std::string(BASICUSAGEENVIRONMENT_LIBRARY_VERSION_STRING);
    std::string ver;
    std::stringstream ss;
    ss << liveVersion << "-client" << LIVE_CLIENT_VERSION_STRING << "."
       << LIVE_CLIENT_BUILD_DATE;
    ss >> ver;
    return ver;
}

bool LiveRtspClient::start() {
    if (bStart) {
        return true;
    }
    if (rtsp_url_.empty() || rtsp_url_.find("rtsp://") != 0) {
        LIVE_LOG(ERROR) << "rtsp url error : [" << rtsp_url_ << "]";
        return false;
    }
    if (env == nullptr || scheduler == nullptr) {
        LIVE_LOG(ERROR) << "env or scheduler nullptr error";
        return false;
    }
    u32RecvFrameCount = 0;
    u32RecvDataBit = 0;
    fps_rt_ = 0;
    fps_average_ = 0.0;
    bitrate_rt_ = 0;
    bitrate_average_ = 0;
    eventLoopWatchVariable = 0;
    liveRtspTid = std::thread(std::bind(&LiveRtspClient::liveRtspThread, this));
    liveAliveTid =
        std::thread(std::bind(&LiveRtspClient::liveRtspAliveThread, this));
    bStart = true;
    is_alive_ = true;
    return true;
}

bool LiveRtspClient::stop() {
    if (!bStart) {
        return true;
    }
    eventLoopWatchVariable = 1;
    liveAliveTid.join();
    liveRtspTid.join();
    bStart = false;
    is_alive_ = false;
    return true;
}

void LiveRtspClient::onRecvRtspData(const std::string url,
                                    const LiveRtspFrame::SPtr& rtspFrame) {
    std::unique_lock<std::mutex> lock(liveMutex);
    if (rtspFrame.get() == nullptr) {
        std::cout << "rtspFrame nullptr error";
        return;
    }
    if (rtspFrame->codecID() == LiveRtspFrame::LIVE_CODEC_ID_H264 ||
        rtspFrame->codecID() == LiveRtspFrame::LIVE_CODEC_ID_H265) {
        videoCodec_ = rtspFrame->codecID();
    } else if (rtspFrame->codecID() >= LiveRtspFrame::LIVE_CODEC_ID_PCMA &&
               rtspFrame->codecID() <= LiveRtspFrame::LIVE_CODEC_ID_AAC) {
        audioCodec_ = rtspFrame->codecID();
    }
    u32RecvFrameCount++;
    u32RecvDataBit += (rtspFrame->size() >> 7);
    if (u32RecvDataBit > 0xFFFFFFFF) {
        u32RecvDataBit = 0;
    }
    if (u32RecvFrameCount > 0xFFFFFFFF) {
        u32RecvFrameCount = 0;
    }
    if (callback_) {
        callback_(url, rtspFrame);
    }
}

void LiveRtspClient::liveRtspThread() {
    char rtspName[128] = {0};
    snprintf(rtspName, sizeof(rtspName), "rtsp_%ld",
             LiveUtils::getTickCount_(LiveUtils::LEVEL_MICROSEC));
    openURL(*env, rtspName, rtsp_url_.c_str(),
            std::bind(&LiveRtspClient::onRecvRtspData, this,
                      std::placeholders::_1, std::placeholders::_2),
            bUseTcpOverRtp);
    env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
    LIVE_LOG(TRACE, true) << " [" << rtsp_url_ << "] Exit";
}

void LiveRtspClient::liveRtspAliveThread() {
    uint32_t u32RecvFrameCountSave = 0;
    uint32_t u32CycleCount = 0;
    uint32_t u32RecvDataBitSave = 0;
    u32RecvFrameCount = 0;
    u32RecvDataBit = 0;
    is_alive_ = true;
    while (eventLoopWatchVariable == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!is_alive_) continue;
        if (u32CycleCount % 3 == 2) {
            if (u32RecvFrameCount == u32RecvFrameCountSave) {
                liveMutex.lock();
                is_alive_ = false;
                u32RecvDataBit = 0;
                u32RecvFrameCount = 0;
                if (serverLost_) {
                    serverLost_(rtsp_url_);
                }
                liveMutex.unlock();
                continue;
            } else {
                is_alive_ = true;
            }
        }
        u32CycleCount++;
        if (u32CycleCount > 0xFFFFFFFF) {
            liveMutex.lock();
            u32CycleCount = 0;
            u32RecvDataBit = 0;
            u32RecvFrameCount = 0;
            u32RecvFrameCountSave = 0;
            u32RecvDataBitSave = 0;
            liveMutex.unlock();
        }
        uint32_t value = (u32RecvFrameCount - u32RecvFrameCountSave);
        if (value < 65535 && value > 0) {
            fps_rt_ = value;
        }
        value = (u32RecvDataBit - u32RecvDataBitSave);
        if (value < 0xFFFFFF && value > 0) {
            bitrate_rt_ = value;
        }
        u32RecvDataBitSave = u32RecvDataBit;
        u32RecvFrameCountSave = u32RecvFrameCount;
        fps_average_ = (u32RecvFrameCountSave * 1.0) / (u32CycleCount * 1.0);
        bitrate_average_ = (u32RecvDataBitSave * 1.0) / (u32CycleCount * 1.0);
    }
    LIVE_LOG(TRACE, true) << " [" << rtsp_url_ << "] Exit";
}

void LiveRtspClient::out(std::ostream& os) const {
    os << rtsp_url_ << " [" << is_alive_ << "]" << std::endl;
    os << "video codec: (" << LiveRtspFrame::codec(videoCodec_) << ")"
       << std::endl;
    os << "fps realtime: (" << fps_rt_ << ")" << std::endl;
    os << "fps average: (" << std::fixed << std::setprecision(2) << fps_average_
       << ")" << std::endl;
    os << "bitrate realtime: (" << bitrate_rt_ << "kbps)" << std::endl;
    os << "bitrate average: (" << std::fixed << std::setprecision(2)
       << bitrate_average_ << "kbps)" << std::endl;
    os << "audio codec: (" << LiveRtspFrame::codec(audioCodec_) << ")"
       << std::endl;
}