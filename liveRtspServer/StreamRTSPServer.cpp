/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-09 15:30:03
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-17 11:22:54
 */

#include "StreamRTSPServer.hh"
#include <functional>
#include <iostream>
#include <algorithm>
#include <string.h>

#include "ByteStreamFrameSource.hh"
#include "GroupsockHelper.hh"
#include "H264VideoStreamServerMediaSubsession.hh"
#include "H265VideoStreamServerMediaSubsession.hh"
#include "LiveCommon.hh"
#include "LiveLogger.hh"
#include "liveMedia.hh"

std::ostream& operator<<(std::ostream& os, const unsigned char& c) {
    // 在这里可以定义你希望的输出格式
    // 例如，将其作为整数输出
    os << static_cast<int>(c);
    return os;
}

StreamRTSPServer* StreamRTSPServer::createNew(
    UsageEnvironment& env, unsigned ourPort,
    UserAuthenticationDatabase* authDatabase, const char* streamName,
    unsigned videoCodec, unsigned audioCodec, unsigned fps,
    unsigned reclamationTestSeconds) {
    Port ourPort_ = ourPort;
    int ourSocketIPv4 = setUpOurSocket(env, ourPort_, AF_INET);
    int ourSocketIPv6 = setUpOurSocket(env, ourPort_, AF_INET6);
    if (ourSocketIPv4 < 0 && ourSocketIPv6 < 0) return NULL;
    char fStreamName_[64] = {0};
    if (streamName == NULL || strlen(streamName) == 0) {
        strcpy(fStreamName_, "liveRtspServer");
    } else {
        if (strlen(streamName) > sizeof(fStreamName)) {
            env << "StreamRTSPServer::StreamRTSPServer() streamName is too "
                   "long, max length is 64\n";
            exit(1);
        } else {
            strcpy(fStreamName_, streamName);
        }
    }
    return new StreamRTSPServer(
        env, ourSocketIPv4, ourSocketIPv6, ourPort_, authDatabase, fStreamName_,
        videoCodec, audioCodec, fps, reclamationTestSeconds);
}

StreamRTSPServer::StreamRTSPServer(UsageEnvironment& env_, int ourSocketIPv4,
                                   int ourSocketIPv6, Port ourPort,
                                   UserAuthenticationDatabase* authDatabase,
                                   const char* streamName, unsigned videoCodec,
                                   unsigned audioCodec, unsigned fps,
                                   unsigned reclamationTestSeconds)
    : RTSPServer(env_, ourSocketIPv4, ourSocketIPv6, ourPort, authDatabase,
                 reclamationTestSeconds, streamName),
      fVideoCodec(videoCodec),
      fAudioCodec(audioCodec),
      fFps(fps){
    memset(fStreamName, 0, sizeof(fStreamName));
    if (strlen(streamName) > sizeof(fStreamName)) {
        env_ << "StreamRTSPServer::StreamRTSPServer() streamName is too long, "
                "max length is 64\n";
        exit(1);
    }
    strcpy(fStreamName, streamName);
    memset(rtspURLPrefix_ipv4, 0, sizeof(rtspURLPrefix_ipv4));
    memset(rtspURLPrefix_ipv6, 0, sizeof(rtspURLPrefix_ipv6));
    env = &env_;
    fIDRFrame = new unsigned char[LiveRtspFrame::MAX_FRAME_SIZE];
    if(fIDRFrame == NULL){
        LIVE_LOG(ERROR)<<"new fIDRFrame failed";
        throw std::runtime_error("fIDRFrame malloc error");
    }
    fIDRFrameSize = 0;
    haveSendIDRFrame = False;
    fFrameSources.clear();
}

StreamRTSPServer::~StreamRTSPServer() {
    ServerMediaSession* sms = getServerMediaSession(fStreamName);
    if (sms != NULL) {
        removeServerMediaSession(sms);
    }
}

void StreamRTSPServer ::lookupServerMediaSession(
    char const* streamName,
    lookupServerMediaSessionCompletionFunc* completionFunc,
    void* completionClientData, Boolean isFirstLookupInSession) {
    // Next, check whether we already have a "ServerMediaSession" for this file:
    ServerMediaSession* sms = getServerMediaSession(streamName);
    Boolean const smsExists = sms != NULL;
    // Handle the four possibilities for "fileExists" and "smsExists":
    if (smsExists && isFirstLookupInSession) {
        // Remove the existing "ServerMediaSession" and create a new one, in
        // case the underlying file has changed in some way:
        removeServerMediaSession(sms);
        sms = NULL;
    }

    if (sms == NULL) {
        sms = createNewSMS(envir(), streamName);
        addServerMediaSession(sms);
    }
    if (completionFunc != NULL) {
        (*completionFunc)(completionClientData, sms);
    }
}

char* StreamRTSPServer::rtspURLPrefix(int clientSocket, Boolean useIPv6) const {
    struct sockaddr_storage ourAddress;

    if (clientSocket < 0) {
        // Use our default IP address in the URL:
        if (!useIPv6) {  // IPv4
            ourAddress.ss_family = AF_INET;
            ((sockaddr_in&)ourAddress).sin_addr.s_addr =
                ourIPv4Address(envir());
        } else {
            ourAddress.ss_family = AF_INET6;
            ipv6AddressBits const& ourAddr6 = ourIPv6Address(envir());
            for (unsigned i = 0; i < 16; ++i)
                ((sockaddr_in6&)ourAddress).sin6_addr.s6_addr[i] = ourAddr6[i];
        }
    } else {
        SOCKLEN_T namelen = sizeof ourAddress;

        getsockname(clientSocket, (struct sockaddr*)&ourAddress, &namelen);
    }

    char urlBuffer[256] = {
        0};  // more than big enough for "rtsp://<ip-address>:<port>/"

    char const* addressPrefixInURL =
        ourAddress.ss_family == AF_INET6 ? "[" : "";
    char const* addressSuffixInURL =
        ourAddress.ss_family == AF_INET6 ? "]" : "";

    portNumBits defaultPortNum = fOurConnectionsUseTLS ? 322 : 554;
    portNumBits portNumHostOrder = ntohs(fServerPort.num());
    if (portNumHostOrder == defaultPortNum) {
        if (strlen(fUrlSuffix) > 0) {
            sprintf(urlBuffer, "rtsp%s://%s%s%s/%s",
                    fOurConnectionsUseTLS ? "s" : "", addressPrefixInURL,
                    AddressString(ourAddress).val(), addressSuffixInURL,
                    fUrlSuffix);
        } else {
            sprintf(urlBuffer, "rtsp%s://%s%s%s",
                    fOurConnectionsUseTLS ? "s" : "", addressPrefixInURL,
                    AddressString(ourAddress).val(), addressSuffixInURL);
        }
    } else {
        if (strlen(fUrlSuffix) > 0) {
            sprintf(urlBuffer, "rtsp%s://%s%s%s:%hu/%s",
                    fOurConnectionsUseTLS ? "s" : "", addressPrefixInURL,
                    AddressString(ourAddress).val(), addressSuffixInURL,
                    portNumHostOrder, fUrlSuffix);
        } else {
            sprintf(urlBuffer, "rtsp%s://%s%s%s:%hu",
                    fOurConnectionsUseTLS ? "s" : "", addressPrefixInURL,
                    AddressString(ourAddress).val(), addressSuffixInURL,
                    portNumHostOrder);
        }
    }

    return strDup(urlBuffer);
}

ServerMediaSession* StreamRTSPServer::createNewSMS(UsageEnvironment& env,
                                                   const char* streamName) {
    Boolean const reuseSource = False;
    OutPacketBuffer::maxSize =
        (1 << 19);  // allow for some possibly large H.264 frames
    ServerMediaSession* sms = NULL;
    ServerMediaSubsession* video_subsession = NULL;
    ServerMediaSubsession* audio_subsession = NULL;
    if (fVideoCodec != LiveRtspFrame::LIVE_CODEC_ID_H264 &&
        fVideoCodec != LiveRtspFrame::LIVE_CODEC_ID_HEVC) {
        LIVE_LOG(ERROR)
            << "StreamRTSPServer::create new sms() unsupport video codec";
        exit(1);
    }
    auto func_ = std::bind(&StreamRTSPServer::onFrameSourceStateChanged, this,
                            std::placeholders::_1, std::placeholders::_2);
    if (fVideoCodec == LiveRtspFrame::LIVE_CODEC_ID_H264) {
        std::string description =
            "H.264 Video, streamed by the LIVE555 Media Server";
        sms = ServerMediaSession::createNew(env, streamName, streamName,
                                            description.c_str());
        video_subsession = H264VideoStreamServerMediaSubsession::createNew(
            env, 
            fFps,
            func_,
            reuseSource);
    } else if (fVideoCodec == LiveRtspFrame::LIVE_CODEC_ID_HEVC) {
        std::string description =
            "H.265 Video, streamed by the LIVE555 Media Server";
        sms = ServerMediaSession::createNew(env, streamName, streamName,
                                            description.c_str());
        video_subsession = H265VideoStreamServerMediaSubsession::createNew(
            env, 
            fFps, 
            func_, 
            reuseSource);
    } else {
        env << "StreamRTSPServer::create new sms unsupport video codec\n";
        return NULL;
    }
    if (video_subsession == NULL) return NULL;
    sms->addSubsession(video_subsession);
    envir() << "create new SMS ok...\n";
    return sms;
}

bool StreamRTSPServer::setVideoData(const unsigned char* buf, int len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (buf == NULL || len <= 0) return false;
    if(!haveSendIDRFrame && ifIDRFrame(buf, len)){
        memcpy(fIDRFrame, buf, len);
        fIDRFrameSize = len;
    }
    haveSendIDRFrame = False;
    for(auto itor = fFrameSources.begin(); itor != fFrameSources.end(); itor++){
        ByteStreamFrameSource* frameSource = *itor;
        if(frameSource == NULL){
            LIVE_LOG(ERROR)<< "frameSource is NULL";
            continue;
        }
        haveSendIDRFrame = True;
        frameSource->doPutIDRFrames(fIDRFrame, fIDRFrameSize);
        frameSource->doPutFrames(buf, len, ifIDRFrame(buf, len));
    }
    return true;
}

bool StreamRTSPServer::setAudioData(const unsigned char* buf, int len) {
    if (buf == NULL || len <= 0) return false;
    
    return true;
}

const char* StreamRTSPServer::RtspUrl(unsigned ipv6) {
    if (weHaveAnIPv4Address(*env)) {
        char* rtspURLPrefix = ipv4rtspURLPrefix();
        if (rtspURLPrefix == NULL) {
            *env << "StreamRTSPServer::StreamRTSPServer() ipv4rtspURLPrefix() "
                    "failed\n";
            exit(1);
        }
        if (strlen(rtspURLPrefix) > (sizeof(rtspURLPrefix_ipv4) - 1)) {
            *env << "StreamRTSPServer::StreamRTSPServer() rtspURLPrefix is too "
                    "long, max length is 64\n";
            exit(1);
        }
        strcpy(rtspURLPrefix_ipv4, rtspURLPrefix);
        delete[] rtspURLPrefix;
    }
    if (weHaveAnIPv6Address(*env)) {
        char* rtspURLPrefix = ipv6rtspURLPrefix();
        if (rtspURLPrefix == NULL) {
            *env << "StreamRTSPServer::StreamRTSPServer() ipv4rtspURLPrefix() "
                    "failed\n";
            exit(1);
        }
        if (strlen(rtspURLPrefix) > (sizeof(rtspURLPrefix_ipv6) - 1)) {
            *env << "StreamRTSPServer::StreamRTSPServer() rtspURLPrefix is too "
                    "long, max length is 64\n";
            exit(1);
        }
        strcpy(rtspURLPrefix_ipv6, rtspURLPrefix);
        delete[] rtspURLPrefix;
    }
    if (ipv6) {
        return rtspURLPrefix_ipv6;
    }
    return rtspURLPrefix_ipv4;
}

void StreamRTSPServer::onFrameSourceStateChanged(void* clientData, Boolean state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(clientData == NULL){
        return;
    }
    ByteStreamFrameSource* framedSource = (ByteStreamFrameSource*)clientData;
    if(framedSource == NULL){
        return;
    }
    if(state == True){
        fFrameSources.push_back(framedSource);
    }else{
        auto itor = std::find(fFrameSources.begin(), fFrameSources.end(), framedSource);
        if(itor != fFrameSources.end()){
            fFrameSources.erase(itor);
        }
    }

}

Boolean StreamRTSPServer::ifIDRFrame(const unsigned char *frame, int len)
{
    if(frame == NULL || len <= 0){
        return False;
    }
    if(fVideoCodec == LiveRtspFrame::LIVE_CODEC_ID_H264){
        avc_nalu_header nalu;
        if(frame[0] == 0x00 && frame[1] == 0x00 && frame[2] == 0x00 && frame[3] == 0x01){
            memcpy(&nalu, frame+4, sizeof(nalu));
        }else{
            memcpy(&nalu, frame, sizeof(nalu));
        }
        if(nalu.type == 0x07) {
            // H264 IDR frame contains SPS(0x07/7)/PPS(0x08/8)/SEI(0x06/6) NALU header
            return True;
        }
    }else if(fVideoCodec == LiveRtspFrame::LIVE_CODEC_ID_HEVC){
        hevc_nalu_header nalu;
        if(frame[0] == 0x00 && frame[1] == 0x00 && frame[2] == 0x00 && frame[3] == 0x01){
            memcpy(&nalu, frame+4, sizeof(nalu));
        }else{
            memcpy(&nalu, frame, sizeof(nalu));
        }
        if (nalu.type == 0x20) {
            // HEVC IDR frame contains VPS(0x20/32)/SPS(0x21/33)/PPS(0x22/34) NALU header
            return True;
        } 
    }
    return False;
}