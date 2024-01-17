/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-19 17:52:23
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-15 09:45:20
 */
#include "LiveCommon.hh"

#include <stdio.h>
#include <string.h>

#include "LiveUtils.hh"

LiveRtspFrame::LiveRtspFrame(uint8_t* frame, uint32_t frameSize,
                             LiveCodecID codecID, LiveFrameType type,
                             uint64_t pts) {
    frame_ = nullptr;
    frameSize_ = 0;
    pts_ = 0;
    codecID_ = LIVE_CODEC_ID_NONE;
    if (frame != nullptr && frameSize > 0) {
        frame_ = new uint8_t[frameSize];
        memcpy(frame_, frame, frameSize);
        frameSize_ = frameSize;
        if (pts <= 0) {
            struct timeval tv;
            if (gettimeofday(&tv, NULL) != -1) {
                pts_ = tv.tv_sec * 1000000 + tv.tv_usec;
            }
        } else {
            pts_ = pts;
        }
        frameType_ = type;
        codecID_ = codecID;
    }
}

LiveRtspFrame::~LiveRtspFrame() {
    if (frame_ != nullptr) {
        delete[] frame_;
        frame_ = nullptr;
    }
    frameSize_ = 0;
    pts_ = 0;
    codecID_ = LIVE_CODEC_ID_NONE;
}

LiveRtspFrame::SPtr LiveRtspFrame::construct(uint8_t* frame, uint32_t frameSize,
                                             LiveCodecID codecID,
                                             LiveFrameType type, uint64_t pts) {
    return std::make_shared<LiveRtspFrame>(frame, frameSize, codecID, type,
                                           pts);
}

LiveRtspFrame::LiveRtspFrame(const LiveRtspFrame& other) {
    pts_ = other.pts_;
    codecID_ = other.codecID_;
    frameSize_ = other.frameSize_;
    frameType_ = other.frameType_;
    if (other.frame_ != nullptr) {
        frame_ = new uint8_t[frameSize_];
        memcpy(frame_, other.frame_, frameSize_);
    } else {
        frame_ = nullptr;
    }
}

uint8_t* LiveRtspFrame::frame() { return frame_; }

uint32_t LiveRtspFrame::size() { return frameSize_; }

uint64_t LiveRtspFrame::pts() { return pts_; }

int32_t LiveRtspFrame::codecID() { return codecID_; }

std::string LiveRtspFrame::codec(int32_t id) {
    switch (id) {
        case LIVE_CODEC_ID_NONE:
            return "none";
        case LIVE_CODEC_ID_PCMA:
            return "PCMA";
        case LIVE_CODEC_ID_PCMU:
            return "PCMU";
        case LIVE_CODEC_ID_AAC:
            return "AAC";
        case LIVE_CODEC_ID_G711:
            return "G711";
        case LIVE_CODEC_ID_H264:
            return "H264";
        case LIVE_CODEC_ID_HEVC:
            return "H265";
    }
    return "unknown";
}

LiveRtspFrame::LiveCodecID LiveRtspFrame::codec(const std::string& codec) {
    if (codec.empty()) return LIVE_CODEC_ID_NONE;
    if (LiveUtils::compareNoCase(codec, "PCMA")) {
        return LIVE_CODEC_ID_PCMA;
    } else if (LiveUtils::compareNoCase(codec, "PCMU")) {
        return LIVE_CODEC_ID_PCMU;
    } else if (LiveUtils::compareNoCase(codec, "AAC")) {
        return LIVE_CODEC_ID_AAC;
    } else if (LiveUtils::compareNoCase(codec, "G711")) {
        return LIVE_CODEC_ID_G711;
    } else if (LiveUtils::compareNoCase(codec, "H264") || codec == "264") {
        return LIVE_CODEC_ID_H264;
    } else if (LiveUtils::compareNoCase(codec, "H265") || (codec == "265") ||
               LiveUtils::compareNoCase(codec, "HEVC")) {
        return LIVE_CODEC_ID_HEVC;
    }
    return LIVE_CODEC_ID_NONE;
}

std::string LiveRtspFrame::codec() const { return codec(codecID_); }

int32_t LiveRtspFrame::frameTypeID() { return frameType_; }

std::string LiveRtspFrame::frameType() const { return frameType(frameType_); }

std::string LiveRtspFrame::frameType(int32_t id) {
    switch (id) {
        case LIVE_FRAME_TYPE_NONE:
            return "NONE";
        case LIVE_FRAME_TYPE_PCMA:
            return "PCMA";
        case LIVE_FRAME_TYPE_PCMU:
            return "PCMU";
        case LIVE_FRAME_TYPE_G711:
            return "G711";
        case LIVE_FRAME_TYPE_AAC:
            return "AAC";
        case LIVE_FRAME_NAL_VPS:
            return "VPS";
        case LIVE_FRAME_NAL_PPS:
            return "PPS";
        case LIVE_FRAME_NAL_SPS:
            return "SPS";
        case LIVE_FRAME_NAL_I:
            return "I";
        case LIVE_FRAME_NAL_P:
            return "P";
        case LIVE_FRAME_NAL_B:
            return "B";
        case LIVE_FRAME_NAL_SEI:
            return "SEI";
        case LIVE_FRAME_NAL_AUD:
            return "AUD";
        case LIVE_FRAME_NAL_FILLER:
            return "FILLER";
    }
    return "unknown";
}

void LiveRtspFrame::out(std::ostream& os, uint32_t count) const {
    if (count <= 0 || count > frameSize_) count = frameSize_;
    if (frame_ != nullptr && count > 0) {
        char* buf = new char[count * 3 + 1];
        if (buf != nullptr) {
            memset(buf, 0, count * 3 + 1);
            for (uint32_t i = 0; i < count; ++i) {
                snprintf(buf + i * 3, 4, "%02x ", (frame_[i] & 0xff));
            }
            os << buf;
            delete[] buf;
            buf = nullptr;
        }
    }
    uint64_t millTick = pts_ / 1000000;
    uint64_t microTick = pts_ % 1000000;
    os << "[" << codec() << " " << frameType() << " size:" << frameSize_
       << " pts:" << millTick << "." << microTick << "]";
    os << std::endl;
}

LiveRtspFrame& LiveRtspFrame::operator=(const LiveRtspFrame& other) {
    if (this == &other) return *this;

    if (frame_ != nullptr) {
        delete[] frame_;
        frame_ = nullptr;
    }
    pts_ = other.pts_;
    codecID_ = other.codecID_;
    frameSize_ = other.frameSize_;
    frameType_ = other.frameType_;
    frame_ = new uint8_t[frameSize_ + 1];
    memcpy(frame_, other.frame_, frameSize_);
    frame_[frameSize_] = 0x0;
    return *this;
}

bool LiveRtspFrame::operator==(const LiveRtspFrame& other) const {
    if (frameSize_ != other.frameSize_ || pts_ != other.pts_ ||
        codecID_ != other.codecID_ || frameType_ != other.frameType_) {
        return false;
    }
    return memcmp(frame_, other.frame_, frameSize_) == 0;
}
