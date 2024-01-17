/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-09 08:27:36
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-17 10:14:22
 */
#include "liveRtspServer.h"

#include <iostream>
#include <sstream>

#include "BasicUsageEnvironment_version.hh"
#include "LiveLogger.hh"
#include "LiveVersion.hh"

LiveRtspServer::LiveRtspServer(LiveRtspFrame::LiveCodecID videoCodec,
                               LiveRtspFrame::LiveCodecID audioCodec,
                               unsigned fps, unsigned port,
                               const std::string &suffix,
                               const std::string &username,
                               const std::string &password)
    : videoCodec_(videoCodec),
      audioCodec_(audioCodec),
      fps_(fps),
      port_(port),
      username_(username),
      password_(password) {
    if (fps <= 0) fps_ = 60;
    if (port <= 0) port_ = 8554;
    if (username.empty() || password.empty()) {
        authDB = NULL;
    } else {
        authDB = new UserAuthenticationDatabase;
        authDB->addUserRecord(username.c_str(), password.c_str());
    }
    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    if (env == NULL || scheduler == NULL) {
        LIVE_LOG(ERROR) << "Failed to create BasicUsageEnvironment: ";
        exit(1);
    }
    rtspServer =
        StreamRTSPServer::createNew(*env, port_, authDB, suffix.c_str(),
                                    videoCodec_, audioCodec_, fps_, 65);
    if (rtspServer == NULL) {
        LIVE_LOG(ERROR) << "Failed to create RTSP server: "
                        << env->getResultMsg();
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }
    bStart = false;
    *env << "LIVE555 Media Server\n";
    *env << "\tversion " << version().c_str() << "\n";
}

LiveRtspServer::LiveRtspServer(const std::string &videoCodec_,
                               const std::string &audioCodec_, unsigned fps,
                               unsigned port, const std::string &suffix,
                               const std::string &username,
                               const std::string &password) {
    auto videoCodec = LiveRtspFrame::codec(videoCodec_);
    auto audioCodec = LiveRtspFrame::codec(audioCodec_);
    if (videoCodec == LiveRtspFrame::LIVE_CODEC_ID_NONE) {
        videoCodec = LiveRtspFrame::LIVE_CODEC_ID_H264;
    }
    port_ = port;
    fps_ = fps;
    if (fps <= 0) fps_ = 60;
    if (port <= 0) port_ = 8554;
    if (username.empty() || password.empty()) {
        authDB = NULL;
    } else {
        authDB = new UserAuthenticationDatabase;
        authDB->addUserRecord(username.c_str(), password.c_str());
    }
    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    if (env == NULL || scheduler == NULL) {
        LIVE_LOG(ERROR) << "Failed to create BasicUsageEnvironment: ";
        exit(1);
    }
    rtspServer =
        StreamRTSPServer::createNew(*env, port_, authDB, suffix.c_str(),
                                    videoCodec, audioCodec, fps_, 65);
    if (rtspServer == NULL) {
        LIVE_LOG(ERROR) << "Failed to create RTSP server: "
                        << env->getResultMsg();
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }
    bStart = false;
    *env << "LIVE555 Media Server version " << version().c_str() << "\n";
}

LiveRtspServer::~LiveRtspServer() { stop(); }

void LiveRtspServer::start() {
    if (bStart) return;
    eventLoopWatchVariable = 0;
    liveRtspTid = std::thread(std::bind(&LiveRtspServer::liveRtspThread, this));
    bStart = true;
}

void LiveRtspServer::stop() {
    if (!bStart) return;
    eventLoopWatchVariable = 1;
    liveRtspTid.join();
}

void LiveRtspServer::liveRtspThread() {
    env->taskScheduler().doEventLoop(
        &eventLoopWatchVariable);  // does not return
}

std::shared_ptr<LiveRtspServer> LiveRtspServer::construct(
    LiveRtspFrame::LiveCodecID videoCodec,
    LiveRtspFrame::LiveCodecID audioCodec, unsigned fps, unsigned port,
    const std::string &suffix, const std::string &username,
    const std::string &password) {
    return std::make_shared<LiveRtspServer>(videoCodec, audioCodec, fps, port,
                                            username, password);
}

std::shared_ptr<LiveRtspServer> LiveRtspServer::construct(
    const std::string &videoCodec_, const std::string &audioCodec_,
    unsigned fps, unsigned port, const std::string &suffix,
    const std::string &username, const std::string &password) {
    return std::make_shared<LiveRtspServer>(videoCodec_, audioCodec_, fps, port,
                                            suffix, username, password);
}

const std::string LiveRtspServer::version() {
    std::string liveVersion =
        std::string("live") +
        std::string(BASICUSAGEENVIRONMENT_LIBRARY_VERSION_STRING);
    std::string ver;
    std::stringstream ss;
    ss << liveVersion << "-server" << LIVE_SERVER_VERSION_STRING << "."
       << LIVE_SERVER_BUILD_DATE;
    ss >> ver;
    return ver;
}

bool LiveRtspServer::sendVideoData(const unsigned char *buf, int len) {
    auto steamServer = dynamic_cast<StreamRTSPServer *>(rtspServer);
    if (steamServer == NULL) return false;
    return steamServer->setVideoData(buf, len);
}

bool LiveRtspServer::sendAudioData(const unsigned char *buf, int len) {
    auto steamServer = dynamic_cast<StreamRTSPServer *>(rtspServer);
    if (steamServer == NULL) return false;
    return steamServer->setAudioData(buf, len);
}

const char *LiveRtspServer::RtspUrl(int ipv6) {
    auto steamServer = dynamic_cast<StreamRTSPServer *>(rtspServer);
    if (steamServer == NULL) return NULL;
    return steamServer->RtspUrl(ipv6);
}