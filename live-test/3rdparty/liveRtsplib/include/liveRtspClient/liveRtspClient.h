/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-17 14:18:04
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-09 14:46:31
 */
#ifndef _LIVE_RTSP_CLIENT_HH_
#define _LIVE_RTSP_CLIENT_HH_

#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "BasicUsageEnvironment.hh"
#include "BasicUsageEnvironment_version.hh"
#include "LiveCommon.hh"
#include "liveMedia.hh"

typedef std::function<void(const std::string& url,
                           const LiveRtspFrame::SPtr& rtspFrame)>
    liveRtspDataCallBack;
typedef std::function<void(const std::string& url)> liveRtspServerLost;

class LiveRtspClient {
public:
    LiveRtspClient(const std::string& rtsp_url, liveRtspDataCallBack callback,
                   liveRtspServerLost serverLost, bool bRtpOverTcp = false);
    ~LiveRtspClient();
    bool start();
    bool stop();
    static void setRecvDebug(bool bDebug_) { bRecvDebug = bDebug_; }
    uint32_t fps_realtime() const { return fps_rt_; }
    uint32_t bitrate_realtime() const { return bitrate_rt_; }
    double fps_average() const { return fps_average_; }
    double bitrate_average() const { return bitrate_average_; }
    std::string url() const { return rtsp_url_; }
    bool is_alive() const { return is_alive_; }
    static const std::string version();
    void out(std::ostream& os) const;
    friend std::ostream& operator<<(std::ostream& os,
                                    const LiveRtspClient& value) {
        value.out(os);
        return os;
    }

private:
    void liveRtspThread();
    void liveRtspAliveThread();
    void onRecvRtspData(const std::string url,
                        const LiveRtspFrame::SPtr& rtspFrame);

private:
    liveRtspDataCallBack callback_;
    liveRtspServerLost serverLost_;
    std::string rtsp_url_;
    bool is_alive_;
    bool bStart;
    bool bUseTcpOverRtp;
    std::mutex liveMutex;
    uint32_t fps_rt_;
    double fps_average_;
    uint32_t bitrate_rt_;
    double bitrate_average_;
    uint32_t u32RecvFrameCount;
    uint32_t u32RecvDataBit;
    int32_t videoCodec_;
    int32_t audioCodec_;
    char eventLoopWatchVariable;
    std::thread liveRtspTid;
    std::thread liveAliveTid;
    TaskScheduler* scheduler;
    UsageEnvironment* env;

public:
    static bool bRecvDebug;
};

#endif  // _LIVE_RTSP_CLIENT_HH_