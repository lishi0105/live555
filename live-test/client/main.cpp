/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-30 15:07:59
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-11 14:28:47
 */

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "LiveLogger.hh"
#include "liveRtspDecoder.h"

#define DISPLAY_
#ifdef DISPLAY_
#include "YUVRenderer.h"
#endif

std::condition_variable cv;
std::mutex cv_m;
bool data_ready = false;
static bool g_bRun = true;
void onGetRtspDecodeData(const uint8_t *dstYUV, int32_t dstW, int32_t dstH,
                         int32_t dstStride, int32_t format, void *pUser) {
    if (pUser == nullptr) return;
        // LIVE_LOG(INFO)<<"onGetRtspDecodeData "<<dstW<<" "<<dstH<<"
        // "<<dstStride<<" "<<format;
#ifdef DISPLAY_
    YUVRenderer *pRenderer = (YUVRenderer *)pUser;
    pRenderer->setYUVData(dstYUV, dstW, dstH);
#endif
    std::lock_guard<std::mutex> lk(cv_m);
    data_ready = true;
    cv.notify_one();
}

void onServerLost(const std::string &url) { LIVE_LOG(ERROR) << url << " lost"; }

void onGetFrameData(const std::string &url, const LiveRtspFrame::SPtr &pdata) {
    LIVE_LOG(TRACE) << url << " " << pdata->size() << " " << pdata->codec()
                    << std::endl;
}

void rtspClientThread(const std::string &rtspUrl) {
    LiveRtspClient *pClient = nullptr;

    // 这里可以使用 rtspUrl
    LIVE_LOG(INFO) << rtspUrl;
    pClient = new LiveRtspClient(rtspUrl, onGetFrameData, onServerLost, true);

    pClient->start();
    while (g_bRun) {  // 持续运行线程，直到外部停止
        std::this_thread::sleep_for(std::chrono::seconds(3));
        LIVE_LOG(INFO) << *pClient;
        // pClient->stop();
        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    delete pClient;  // 确保正确清理内存
}

void rtspCycleTest() {
    std::vector<std::string> rtspUrls = {
        "rtsp://192.168.118.139/output_720p4M.264"};
    std::cout << LiveRtspClient::version() << std::endl;
    for (auto &url : rtspUrls) {
        std::thread t(rtspClientThread, url);
        t.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

int main(int argc, char *argv[]) {
#if 1
    std::string rtspUrl = "rtsp://192.168.118.139/output_720p4M_B.264";
    if (argc > 1) {
        std::string rtsp_url_ = argv[1];
        if (rtsp_url_.find("rtsp://") != std::string::npos) {
            rtspUrl = rtsp_url_;
        } else {
            LIVE_LOG(ERROR) << "rtspUrl: \"" << rtsp_url_
                            << "\" not start with \"rtsp\" error";
            return -1;
        }
    }
    LiveRtspClient::setRecvDebug(false);
    std::cout << LiveRtspClient::version() << std::endl;
#ifdef DISPLAY_
    auto pRenderer = new YUVRenderer(960, 540, -1, -1, rtspUrl);
    liveRtspDecoder *pDecoder = new liveRtspDecoder(
        rtspUrl, onGetRtspDecodeData, onServerLost, pRenderer);
#else
    liveRtspDecoder *pDecoder = new liveRtspDecoder(
        rtspUrl, onGetRtspDecodeData, onServerLost, nullptr);
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    pDecoder->start();
    while (g_bRun) {  // 持续运行线程，直到外部停止
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [] { return data_ready; });  // 等待直到data_ready为true
#ifdef DISPLAY_
        pRenderer->render();
#endif
        data_ready = false;
    }
#else
    rtspCycleTest();
    while (1) {  // 持续运行线程，直到外部停止
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
#endif
    return 0;
}