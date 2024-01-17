/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2021-04-02 10:26:23
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-02 09:21:49
 */
#ifndef _VIDEO_DECODER_H_
#define _VIDEO_DECODER_H_
#include <stdint.h>

#include <iostream>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

class videoDecoder {
public:
    typedef enum VideoCodec_ {
        VIDEO_CODEC_H264 = 0x1B,
        VIDEO_CODEC_HEVC = 0xAD,
#define VIDEO_CODEC_H265 VIDEO_CODEC_HEVC
    } VideoCodec;

    typedef enum DecPixelFormat_ {
        DEC_FMT_NONE = -1,
        DEC_FMT_YUV420P,  ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per
                          ///< 2x2 Y samples)
        DEC_FMT_YUYV422,  ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
        DEC_FMT_UYVY422,  ///< packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
        DEC_FMT_YVYU422,  ///< packed YUV 4:2:2, 16bpp, Y0 Cr Y1 Cb
        DEC_FMT_NV12,   ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane
                        ///< for the UV components, which are interleaved (first
                        ///< byte U and the following byte V)
        DEC_FMT_NV21,   ///< as above, but U and V bytes are swapped
        DEC_FMT_RGB24,  ///< packed RGB 8:8:8, 24bpp, RGBRGB...
        DEC_FMT_BGR24,  ///< packed RGB 8:8:8, 24bpp, BGRBGR...

        DEC_FMT_ARGB,  ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
        DEC_FMT_RGBA,  ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        DEC_FMT_ABGR,  ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
        DEC_FMT_BGRA,  ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
    } DecPixelFormat;

public:
    videoDecoder(VideoCodec codec = VIDEO_CODEC_H264);
    virtual ~videoDecoder();
    bool initDecoder(std::string &err);
    bool decodeVideo(unsigned char *src, int srcSize, unsigned char *dst,
                     int32_t &format, int32_t &dstW, int32_t &dstH,
                     std::string &err);
    void uinitDecoder();
    bool EnableHardware(bool bEnable);

private:
    int32_t AVFormat2DecFormat(int);
    bool initHWDevice(AVCodecContext *ctx, const AVCodec *decoder);
    bool decoderPacket(AVCodecContext *avctx, AVPacket *packet,
                       unsigned char *pDst, int *format, std::string &err);

private:
    int decWidth;
    int decHeight;
    AVCodecContext *decCodecCtx_hw;
    AVCodecContext *decCodecCtx_sw;
    enum AVPixelFormat hwPixFmt;
    AVBufferRef *hwDevCtx;
    AVPacket *packet;
    bool bHaveInit;
    bool bUseHardware;
    bool bHardwareEnable;
    bool bFindIdr;
    std::string hardwareErrMsg;
    VideoCodec videoCodec_;
};

#endif  //_VIDEO_DECODER_H_