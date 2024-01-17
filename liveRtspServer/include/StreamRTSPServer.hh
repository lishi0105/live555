/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-09 15:29:48
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-17 11:15:08
 */
#ifndef _STREAM_RTSP_SERVER_HH
#define _STREAM_RTSP_SERVER_HH

#ifndef _RTSP_SERVER_HH
#include "RTSPServer.hh"
#endif
#include <mutex>
#include <vector>
#include <iostream>
#include "ByteStreamFrameSource.hh"

class StreamRTSPServer : public RTSPServer {
public:
    static StreamRTSPServer* createNew(UsageEnvironment& env, unsigned ourPort,
                                       UserAuthenticationDatabase* authDatabase,
                                       const char* streamName,
                                       unsigned videoCodec, unsigned audioCodec,
                                       unsigned fps,
                                       unsigned reclamationTestSeconds = 65);
    bool setVideoData(const unsigned char* buf, int len);
    bool setAudioData(const unsigned char* buf, int len);
    const char* RtspUrl(unsigned ipv6 = 0);
    char* rtspURLPrefix(int clientSocket, Boolean useIPv6) const override;

protected:
    StreamRTSPServer(UsageEnvironment& env, int ourSocketIPv4,
                     int ourSocketIPv6, Port ourPort,
                     UserAuthenticationDatabase* authDatabase,
                     const char* streamName, unsigned videoCodec,
                     unsigned audioCodec, unsigned fps,
                     unsigned reclamationTestSeconds);
    // called only by createNew();
    virtual ~StreamRTSPServer();

protected:
    virtual void lookupServerMediaSession(
        char const* streamName,
        lookupServerMediaSessionCompletionFunc* completionFunc,
        void* completionClientData, Boolean isFirstLookupInSession);

private:
    Boolean ifIDRFrame(const unsigned char *frame, int len);
    ServerMediaSession* createNewSMS(UsageEnvironment& env,
                                     const char* streamName);
    void onFrameSourceStateChanged(void* clientData, Boolean state);

    void onPutData(const unsigned char* buf, int len);

private:
    unsigned fVideoCodec;
    unsigned fAudioCodec;
    unsigned fFps;
    char fStreamName[64];
    char rtspURLPrefix_ipv4[64];
    char rtspURLPrefix_ipv6[64];
    unsigned char* fIDRFrame;
    unsigned fIDRFrameSize;
    UsageEnvironment* env;
    std::mutex m_mutex;
    Boolean haveSendIDRFrame;
    std::vector<ByteStreamFrameSource* > fFrameSources;
};

#endif  // !_STREAM_RTSP_SERVER_HH
