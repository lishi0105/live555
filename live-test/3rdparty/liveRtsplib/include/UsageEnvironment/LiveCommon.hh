/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-17 14:18:26
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-15 09:45:49
 */
#ifndef _LIVE_COMMON_HH_
#define _LIVE_COMMON_HH_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <memory>
typedef struct avc_nalu_header_ {
    unsigned char type : 5;
    unsigned char nal_ref_idc : 2;
    unsigned char forbidden_zero_bit : 1;
} avc_nalu_header;

typedef struct hevc_nalu_header_ {
    unsigned short forbidden_zero_bit : 1;
    unsigned short type : 6;
    unsigned short reserved_sero_bit : 6;
    unsigned short temporal_id_plus : 3;
} hevc_nalu_header;

class LiveRtspFrame {
public:
    static const uint32_t MAX_FRAME_SIZE = (1<<19);
    typedef enum _Live_CodecID {
        LIVE_CODEC_ID_NONE = 0x0,
        LIVE_CODEC_ID_PCMA,
        LIVE_CODEC_ID_PCMU,
        LIVE_CODEC_ID_G711,
        LIVE_CODEC_ID_AAC,
        LIVE_CODEC_ID_H264 = 0x1B,
        LIVE_CODEC_ID_HEVC = 0xAD,
#define LIVE_CODEC_ID_H265 LIVE_CODEC_ID_HEVC
    } LiveCodecID;

    typedef enum _Live_FrameType {
        LIVE_FRAME_TYPE_NONE = 0,
        LIVE_FRAME_TYPE_PCMA,
        LIVE_FRAME_TYPE_PCMU,
        LIVE_FRAME_TYPE_G711,
        LIVE_FRAME_TYPE_AAC,
        LIVE_FRAME_NAL_VPS,  //（视频参数集）
        LIVE_FRAME_NAL_PPS,  //（序列参数集）
        LIVE_FRAME_NAL_SPS,  //（图像参数集）
        LIVE_FRAME_NAL_I,
        LIVE_FRAME_NAL_P,
        LIVE_FRAME_NAL_B,
        LIVE_FRAME_NAL_SEI,
        LIVE_FRAME_NAL_AUD,    //（访问单元分隔符）
        LIVE_FRAME_NAL_CRA,    //（清晰随机访问）
        LIVE_FRAME_NAL_EOS,    //（序列结束）
        LIVE_FRAME_NAL_EOB,    //（流结束）
        LIVE_FRAME_NAL_FILLER  //
    } LiveFrameType;
    typedef std::shared_ptr<LiveRtspFrame> SPtr;
    LiveRtspFrame(uint8_t* frame, uint32_t frameSize, LiveCodecID codecID,
                  LiveFrameType type, uint64_t pts = 0);
    ~LiveRtspFrame();

    static LiveRtspFrame::SPtr construct(uint8_t* frame, uint32_t frameSize,
                                         LiveCodecID codecID,
                                         LiveFrameType type, uint64_t pts = 0);
    LiveRtspFrame(const LiveRtspFrame& other);
    uint8_t* frame();
    uint32_t size();
    uint64_t pts();
    int32_t codecID();
    std::string codec() const;
    static std::string codec(int32_t id);
    static LiveCodecID codec(const std::string& codec);
    int32_t frameTypeID();
    std::string frameType() const;
    static std::string frameType(int32_t id);
    void out(std::ostream& os, uint32_t count = 0) const;
    LiveRtspFrame& operator=(const LiveRtspFrame& other);
    bool operator==(const LiveRtspFrame& other) const;
    friend std::ostream& operator<<(std::ostream& os,
                                    const LiveRtspFrame& value) {
        value.out(os, 8);
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os,
                                    const LiveCodecID& value) {
        os << LiveRtspFrame::codec(value);
        return os;
    }
    friend std::ostream& operator<<(std::ostream& os,
                                    const LiveFrameType& value) {
        os << LiveRtspFrame::frameType(value);
        return os;
    }

private:
    uint8_t* frame_;
    uint32_t frameSize_;
    uint64_t pts_;
    int32_t codecID_;
    int32_t frameType_;
};

#endif  // _CLIENT_COMMON_HH_