/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-31 14:00:04
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-08 11:09:38
 */
#ifndef _LIVE_RTSP_DECODER_H_
#define _LIVE_RTSP_DECODER_H_

#include <functional>
#include <memory>

#include "LiveLogger.hh"
#include "liveRtspClient.h"
#include "videoDecoder.h"

typedef std::function<void(const uint8_t *dstYUV, int32_t dstW, int32_t dstH,
                           int32_t dstStride, int32_t format, void *pUser)>
    liveRtspDecoderCallBack;
typedef std::function<void(const std::string &url)> liveRtspServerLost;

class liveRtspDecoder : public LiveRtspClient {
public:
    liveRtspDecoder(const std::string &url, liveRtspDecoderCallBack callback,
                    liveRtspServerLost serverLost, void *pUser);
    ~liveRtspDecoder();
    void start();
    void stop();

private:
    void onGetFrameData(const std::string &url,
                        const LiveRtspFrame::SPtr &pdata);
    void onServerLost(const std::string &url);

private:
    int32_t rtsp_width;
    int32_t rtsp_height;
    bool bStart;
    std::string url_;
    liveRtspDecoderCallBack callback_;
    liveRtspServerLost serverLost_;
    void *pUser_;
    std::shared_ptr<videoDecoder> H264Decoder_;
    std::shared_ptr<videoDecoder> H265Decoder_;
};

#endif  // _LIVE_RTSP_DECODER_H_