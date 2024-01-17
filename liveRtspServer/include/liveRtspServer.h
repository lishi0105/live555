/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-11-09 08:28:03
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-17 11:09:55
 */
#ifndef _LIVE_RTSP_SERVER_H_
#define _LIVE_RTSP_SERVER_H_

#include <iostream>
#include <memory>
#include <thread>

#include "BasicUsageEnvironment.hh"
#include "LiveCommon.hh"
#include "RTSPServer.hh"
#include "StreamRTSPServer.hh"

class LiveRtspServer {
public:
    LiveRtspServer(LiveRtspFrame::LiveCodecID videoCodec,
                   LiveRtspFrame::LiveCodecID audioCodec =
                       LiveRtspFrame::LIVE_CODEC_ID_NONE,
                   unsigned fps = 60, unsigned port = 8554,
                   const std::string &suffix = "",
                   const std::string &username = "",
                   const std::string &password = "");
    LiveRtspServer(const std::string &videoCodec = "h264",
                   const std::string &audioCodec = "none", unsigned fps = 60,
                   unsigned port = 8554, const std::string &suffix = "",
                   const std::string &username = "",
                   const std::string &password = "");
    static std::shared_ptr<LiveRtspServer> construct(
        LiveRtspFrame::LiveCodecID videoCodec,
        LiveRtspFrame::LiveCodecID audioCodec =
            LiveRtspFrame::LIVE_CODEC_ID_NONE,
        unsigned fps = 60, unsigned port = 8554, const std::string &suffix = "",
        const std::string &username = "", const std::string &password = "");
    static std::shared_ptr<LiveRtspServer> construct(
        const std::string &videoCodec = "h264",
        const std::string &audioCodec = "none", unsigned fps = 60,
        unsigned port = 8554, const std::string &suffix = "",
        const std::string &username = "", const std::string &password = "");
    ~LiveRtspServer();

    static const std::string version();

    void start();
    void stop();
    bool sendVideoData(const unsigned char *buf, int len);
    bool sendAudioData(const unsigned char *buf, int len);
    const char *RtspUrl(int ipv6 = 0);

private:
    void liveRtspThread();

private:
    LiveRtspFrame::LiveCodecID videoCodec_;
    LiveRtspFrame::LiveCodecID audioCodec_;
    unsigned fps_;
    unsigned port_;
    std::string username_;
    std::string password_;
    UserAuthenticationDatabase *authDB;
    RTSPServer *rtspServer;
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    char volatile eventLoopWatchVariable;
    std::thread liveRtspTid;
    bool bStart;
};

#endif  // !_LIVE_RTSP_SERVER_HH