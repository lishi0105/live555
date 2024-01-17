/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-31 14:00:19
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-11 14:28:56
 */
#include "liveRtspDecoder.h"

liveRtspDecoder::liveRtspDecoder(const std::string &url,
                                 liveRtspDecoderCallBack callback,
                                 liveRtspServerLost lostCallback, void *pUser)
    : LiveRtspClient(url,
                     std::bind(&liveRtspDecoder::onGetFrameData, this,
                               std::placeholders::_1, std::placeholders::_2),
                     std::bind(&liveRtspDecoder::onServerLost, this,
                               std::placeholders::_1),
                     true) {
    bStart = false;
    rtsp_width = 0;
    rtsp_height = 0;
    url_ = url;
    callback_ = callback;
    serverLost_ = lostCallback;
    pUser_ = pUser;
    H264Decoder_ =
        std::make_shared<videoDecoder>(videoDecoder::VIDEO_CODEC_H264);
    H265Decoder_ =
        std::make_shared<videoDecoder>(videoDecoder::VIDEO_CODEC_H265);
    std::string err = "";
    if (!H264Decoder_->initDecoder(err)) {
        LIVE_LOG(ERROR) << "H264Decoder init failed :" << err;
    }
    if (!H265Decoder_->initDecoder(err)) {
        LIVE_LOG(ERROR) << "H264Decoder init failed :" << err;
    }
    H264Decoder_->EnableHardware(true);
    H265Decoder_->EnableHardware(true);
    LIVE_LOG(INFO) << "liveRtspDecoder init success";
}

liveRtspDecoder::~liveRtspDecoder() {
    callback_ = nullptr;
    stop();
    H264Decoder_->uinitDecoder();
    H265Decoder_->uinitDecoder();
}

void liveRtspDecoder::start() {
    if (bStart) return;
    LiveRtspClient::start();
    bStart = true;
}

void liveRtspDecoder::stop() {
    if (!bStart) return;
    LiveRtspClient::stop();
    bStart = false;
}

void liveRtspDecoder::onGetFrameData(const std::string &url,
                                     const LiveRtspFrame::SPtr &pdata) {
    LIVE_LOG(INFO) << *pdata;
    // static FILE *fp = fopen("test.h265", "wb");
    // if(fp != nullptr){
    //     fwrite(pdata->frame(), 1, pdata->size(), fp);
    //     fflush(fp);
    // }
    // return;
    // auto start = std::chrono::steady_clock::now();
    if (pdata.get() == nullptr) return;
    if (pdata->codecID() != LiveRtspFrame::LIVE_CODEC_ID_H264 &&
        pdata->codecID() != LiveRtspFrame::LIVE_CODEC_ID_H265)
        return;

    uint8_t *frame = pdata->frame();
    uint32_t size = pdata->size();
    uint64_t pts = pdata->pts();
    int32_t width = 0;
    int32_t height = 0;
    int32_t format = 0;
    uint8_t *pYUV420Buffer_ = nullptr;
    if (rtsp_width > 0 && rtsp_height > 0) {
        pYUV420Buffer_ = new uint8_t[rtsp_width * rtsp_height * 3 / 2];
        if (pYUV420Buffer_ == nullptr) {
            LIVE_LOG(ERROR) << "new pYUV420Buffer_ failed";
            return;
        }
    }
    if (pdata->codecID() == LiveRtspFrame::LIVE_CODEC_ID_H264) {
        if (H264Decoder_ == nullptr) return;
        if (pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_I &&
            pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_P &&
            pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_B) {
            return;
        }
        std::string err;
        if (!H264Decoder_->decodeVideo(frame, size, pYUV420Buffer_, format,
                                       width, height, err)) {
            LIVE_LOG(ERROR) << "decodeVideo failed :" << err;
            if (pYUV420Buffer_ != nullptr) {
                delete[] pYUV420Buffer_;
                pYUV420Buffer_ = nullptr;
            }
            return;
        }
    } else if (pdata->codecID() == LiveRtspFrame::LIVE_CODEC_ID_H265) {
        if (H265Decoder_ == nullptr) return;
        if (pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_I &&
            pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_P &&
            pdata->frameTypeID() != LiveRtspFrame::LIVE_FRAME_NAL_B) {
            return;
        }
        std::string err;
        // LIVE_LOG(INFO)<<"decodeVideo "<<size<<" "<<width<<" "<<height;
        if (!H265Decoder_->decodeVideo(frame, size, pYUV420Buffer_, format,
                                       width, height, err)) {
            LIVE_LOG(ERROR) << "decodeVideo failed :" << err;
            if (pYUV420Buffer_ != nullptr) {
                delete[] pYUV420Buffer_;
                pYUV420Buffer_ = nullptr;
            }
            return;
        }
        // LIVE_LOG(INFO)<<"decodeVideo "<<size<<" "<<width<<" "<<height;
    }
    rtsp_width = width;
    rtsp_height = height;
    if (callback_ != nullptr && pYUV420Buffer_ != nullptr) {
        callback_(pYUV420Buffer_, width, height, width, format, pUser_);
    }
    if (pYUV420Buffer_ != nullptr) {
        delete[] pYUV420Buffer_;
        pYUV420Buffer_ = nullptr;
    }
    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end
    // - start).count(); std::cout << "decode cost " << duration << " ms" <<
    // std::endl;
}

void liveRtspDecoder::onServerLost(const std::string &url) {
    if (serverLost_) {
        serverLost_(url);
    }
}