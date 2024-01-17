/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-31 10:26:31
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-03 17:35:15
 */
#include "videoDecoder.h"

videoDecoder::videoDecoder(VideoCodec codec) : videoCodec_(codec) {
    decCodecCtx_sw = NULL;
    decCodecCtx_hw = NULL;
    hwDevCtx = NULL;
    packet = nullptr;
    decWidth = 0;
    decHeight = 0;
    bUseHardware = false;
    bHaveInit = false;
    bFindIdr = false;
    bHardwareEnable = true;
}

videoDecoder::~videoDecoder() { uinitDecoder(); }

bool videoDecoder::initHWDevice(AVCodecContext *ctx, const AVCodec *decoder) {
    int s32Ret = 0;
    std::string hwDevStr = "";
    if (decoder == NULL || ctx == NULL) {
        char err_msg[256] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] decoder or ctx nullptr error",
                 __LINE__);
        hardwareErrMsg = std::string(err_msg);
        return false;
    }
    std::vector<std::string> hwDevs;
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    hwDevs.clear();
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
        // TRACE("%s\n",av_hwdevice_get_type_name(type));
        hwDevs.push_back(av_hwdevice_get_type_name(type));
        s32Ret++;
    }
    if (s32Ret <= 0) return -1;
    for (auto iter = hwDevs.begin(); iter != hwDevs.end(); iter++) {
        std::string itStr = (std::string)*iter;
        if (itStr.compare("dxva2") == 0) {
            hwDevStr = "dxva2";
            break;
        }
    }
    if (hwDevStr.empty()) {
        hwDevStr = hwDevs.at(0);
    }
    type = av_hwdevice_find_type_by_name(hwDevStr.c_str());
    // TRACE("use %s decode \n",hwDevStr.c_str());
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            hardwareErrMsg = "Decoder " + std::string(decoder->name) +
                             " does not support device type " +
                             std::string(av_hwdevice_get_type_name(type));
            return false;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == type) {
            hwPixFmt = config->pix_fmt;
            break;
        }
    }
    ctx->pix_fmt = hwPixFmt;
    s32Ret = av_hwdevice_ctx_create(&hwDevCtx, type, NULL, NULL, 0);
    if (s32Ret < 0) {
        char err_msg[256] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] Failed to create specified HW device.",
                 __LINE__);
        hardwareErrMsg = std::string(err_msg);
        return false;
    }
    ctx->hw_device_ctx = av_buffer_ref(hwDevCtx);
    hardwareErrMsg = "hardware success";
    return true;
}

bool videoDecoder::decoderPacket(AVCodecContext *avctx, AVPacket *packet,
                                 unsigned char *pDst, int *format,
                                 std::string &err) {
    AVFrame *frame = NULL, *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;
    *format = -1;
    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        char buf[256] = {0};
        av_strerror(ret, buf, sizeof(buf));
        char err_msg[512] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] Error sending a packet for decoding:%s",
                 __LINE__, buf);
        err = std::string(err_msg);
        return false;
    }
    while (ret >= 0) {
        if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            char err_msg[256] = {0};
            snprintf(err_msg, sizeof(err_msg),
                     "[videoDecoder.cpp %d] Can not alloc frame", __LINE__);
            err = std::string(err_msg);
            ret = AVERROR(ENOMEM);
            if (frame) {
                av_frame_free(&frame);
                frame = NULL;
            }
            if (sw_frame) {
                av_frame_free(&sw_frame);
                frame = NULL;
            }
            return false;
        }
        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            if (frame) {
                av_frame_free(&frame);
                frame = NULL;
            }
            if (sw_frame) {
                av_frame_free(&sw_frame);
                frame = NULL;
            }
            if (*format == -1) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] decode out format error",
                         __LINE__);
                err = std::string(err_msg);
                return false;
            }
            err = "decode success";
            return true;
        } else if (ret < 0) {
            char err_msg[256] = {0};
            snprintf(err_msg, sizeof(err_msg),
                     "[videoDecoder.cpp %d] Error during decoding", __LINE__);
            err = std::string(err_msg);
            if (frame) {
                av_frame_free(&frame);
                frame = NULL;
            }
            if (sw_frame) {
                av_frame_free(&sw_frame);
                frame = NULL;
            }
            return false;
        }
        if (!bHardwareEnable || !bUseHardware) {
            if (pDst == nullptr) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] pDst nullptr error", __LINE__);
                err = std::string(err_msg);
                if (frame) {
                    av_frame_free(&frame);
                    frame = NULL;
                }
                if (sw_frame) {
                    av_frame_free(&sw_frame);
                    frame = NULL;
                }
                return true;
            }
            int videoWidth = 0, videoHeight = 0, BpS = 0;
            int uvlineStep, uvHei, i;
            unsigned char *PtrY, *PtrV, *PtrU;
            unsigned char *PtrYY, *PtrUU, *PtrVV;
            videoWidth = avctx->width;
            videoHeight = avctx->height;
            PtrYY = frame->data[0];
            PtrUU = frame->data[1];
            PtrVV = frame->data[2];
            PtrY = pDst;
            PtrU = pDst + videoWidth * videoHeight;
            PtrV = pDst + ((videoWidth * videoHeight * 5) >> 2);
            BpS = frame->linesize[0];
            if (BpS < videoHeight) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] frame->linesize[0](%d) < "
                         "videoHeight error",
                         __LINE__, BpS);
                err = std::string(err_msg);
                if (frame) {
                    av_frame_free(&frame);
                    frame = NULL;
                }
                if (sw_frame) {
                    av_frame_free(&sw_frame);
                    frame = NULL;
                }
                return false;
            }
            for (i = 0; i < videoHeight; i++) {
                memcpy(PtrY, PtrYY, videoWidth);
                PtrYY += BpS;
                PtrY += videoWidth;
            }
            BpS = frame->linesize[1];
            uvlineStep = videoWidth >> 1;
            uvHei = videoHeight >> 1;
            for (i = 0; i < uvHei; i++) {
                memcpy(PtrV, PtrVV, uvlineStep);
                PtrVV += BpS;
                PtrV += uvlineStep;
            }

            BpS = frame->linesize[2];
            for (i = 0; i < uvHei; i++) {
                memcpy(PtrU, PtrUU, uvlineStep);
                PtrUU += BpS;
                PtrU += uvlineStep;
            }
            *format = frame->format;
            if (frame) {
                av_frame_free(&frame);
                frame = NULL;
            }
            if (sw_frame) {
                av_frame_free(&sw_frame);
                frame = NULL;
            }
            err = "decode success";
            return true;
        }
        if (frame->format == hwPixFmt) {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                if (frame) {
                    av_frame_free(&frame);
                    frame = NULL;
                }
                if (sw_frame) {
                    av_frame_free(&sw_frame);
                    frame = NULL;
                }
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] transferring the data to "
                         "system memory",
                         __LINE__);
                err = std::string(err_msg);
                return false;
            }
            tmp_frame = sw_frame;
        } else {
            tmp_frame = frame;
        }

        size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format,
                                        tmp_frame->width, tmp_frame->height,
                                        tmp_frame->format);
        if (pDst != nullptr) {
            ret = av_image_copy_to_buffer(
                pDst, size, (const uint8_t *const *)tmp_frame->data,
                (const int *)tmp_frame->linesize,
                (AVPixelFormat)tmp_frame->format, tmp_frame->width,
                tmp_frame->height, tmp_frame->format);
            if (ret < 0) {
                err = "Can not copy image to buffer";
                if (frame) {
                    av_frame_free(&frame);
                    frame = NULL;
                }
                if (sw_frame) {
                    av_frame_free(&sw_frame);
                    frame = NULL;
                }
                return false;
            }
        }
        *format = tmp_frame->format;
        if (frame) {
            av_frame_free(&frame);
            frame = NULL;
        }
        if (sw_frame) {
            av_frame_free(&sw_frame);
            frame = NULL;
        }
    }
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }
    if (sw_frame) {
        av_frame_free(&sw_frame);
        frame = NULL;
    }
    return true;
}

int32_t videoDecoder::AVFormat2DecFormat(int srcFormat) {
    switch (srcFormat) {
        case AV_PIX_FMT_NONE:
            return DEC_FMT_NONE;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            return DEC_FMT_YUV420P;
        case AV_PIX_FMT_YUYV422:
        case AV_PIX_FMT_YUVJ422P:
            return DEC_FMT_YUYV422;
        case AV_PIX_FMT_RGB24:
            return DEC_FMT_RGB24;
        case AV_PIX_FMT_BGR24:
            return DEC_FMT_BGR24;
        case AV_PIX_FMT_UYVY422:
            return DEC_FMT_UYVY422;
        case AV_PIX_FMT_NV12:
            return DEC_FMT_NV12;
        case AV_PIX_FMT_NV21:
            return DEC_FMT_NV21;
        case AV_PIX_FMT_ARGB:
            return DEC_FMT_ARGB;
        case AV_PIX_FMT_RGBA:
            return DEC_FMT_RGBA;
        case AV_PIX_FMT_ABGR:
            return DEC_FMT_ABGR;
        case AV_PIX_FMT_BGRA:
            return DEC_FMT_BGRA;
        case AV_PIX_FMT_YVYU422:
            return DEC_FMT_YVYU422;
    }
    return DEC_FMT_YUV420P;
}

bool videoDecoder::decodeVideo(unsigned char *src, int srcSize,
                               unsigned char *dst, int32_t &dstFormat,
                               int32_t &dstW, int32_t &dstH, std::string &err) {
    int s32Ret = 0, s32Format = 0, s32Type = 0;
    dstFormat = DEC_FMT_NONE;
    if (src == NULL || srcSize < 10) {
        char err_msg[256] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] src nullptr error", __LINE__);
        err = std::string(err_msg);
        return false;
    }
    if (src == NULL || srcSize < 10) {
        char err_msg[256] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] srcSize[%d] < 10 error", __LINE__,
                 srcSize);
        err = std::string(err_msg);
        return false;
    }

    if (!bFindIdr) {
        if (videoCodec_ == VIDEO_CODEC_H264) {
            s32Type = (src[4] & 0x1f);
            if (s32Type == 0x06) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] sei frame don't decode",
                         __LINE__);
                err = std::string(err_msg);
                return true;
            }
            // DBG_PRINT("%x %x %x %x %x %x %x %x %d\n", src[0], src[1], src[2],
            // src[3], src[4], src[5], src[6], src[7],type);
            if (s32Type != 0x07 && s32Type != 0x08 && s32Type != 0x05) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] h264 not find idr frame",
                         __LINE__);
                err = std::string(err_msg);
                return false;
            }
        } else {
            s32Type = ((src[4] & 0x7E) >> 1);
            // DBG_PRINT("%x %x %x %x %x %x %x %x %d\n", src[0], src[1], src[2],
            // src[3], src[4], src[5], src[6], src[7],type);
            if (s32Type != 0x20 && s32Type != 0x21 && s32Type != 0x22 &&
                s32Type != 0x13) {
                char err_msg[256] = {0};
                snprintf(err_msg, sizeof(err_msg),
                         "[videoDecoder.cpp %d] h265 not find idr frame",
                         __LINE__);
                err = std::string(err_msg);
                return false;
            }
        }
        bFindIdr = true;
    }
    packet->data = src;
    packet->size = srcSize;
    if (bHardwareEnable && bUseHardware) {
        s32Ret = decoderPacket(decCodecCtx_hw, packet, dst, &s32Format, err);
    } else {
        s32Ret = decoderPacket(decCodecCtx_sw, packet, dst, &s32Format, err);
    }
    if (!s32Ret) {
        decWidth = 0;
        decHeight = 0;
        return false;
    }
    if (bHardwareEnable && bUseHardware) {
        decWidth = decCodecCtx_hw->width;
        decHeight = decCodecCtx_hw->height;
    } else {
        decWidth = decCodecCtx_sw->width;
        decHeight = decCodecCtx_sw->height;
    }
    if (decWidth <= 0 || decHeight <= 0) {
        char err_msg[1024] = {0};
        snprintf(err_msg, sizeof(err_msg),
                 "[videoDecoder.cpp %d] decode width(%d) or height(%d) error",
                 __LINE__, decWidth, decHeight);
        err = std::string(err_msg);
        return false;
    }
    dstW = decWidth;
    dstH = decHeight;
    dstFormat = AVFormat2DecFormat(s32Format);  // AV_PIX_FMT_YUVJ420P
    // DBG_PRINT("AV_PIX_FMT_YUVJ420P %d s32Format %d dstFormat %d\n",
    // AV_PIX_FMT_YUVJ420P, s32Format, *dstFormat);
    err = "decode success";
    return true;
}

bool videoDecoder::EnableHardware(bool enable_) {
    if (!bHardwareEnable) {
        return false;
    }
    if (bUseHardware == enable_) {
        return true;
    }
    bUseHardware = enable_;
    return true;
}

bool videoDecoder::initDecoder(std::string &err) {
    const AVCodec *dec_hw = NULL, *dec_sw = NULL;
    if (bHaveInit) {
        err = "already init";
        return false;
    }
    packet = av_packet_alloc();
    if (!packet) {
        err = "Failed to alloc packet";
        return false;
    }
    if (videoCodec_ == VIDEO_CODEC_H264) {
        dec_hw = avcodec_find_decoder(AV_CODEC_ID_H264);
        dec_sw = avcodec_find_decoder(AV_CODEC_ID_H264);
    } else if (videoCodec_ == VIDEO_CODEC_H265) {
        dec_hw = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        dec_sw = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }
    if (!dec_hw || !dec_sw) {
        err = "Failed to find codec" +
              std::string(av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }

    decCodecCtx_hw = avcodec_alloc_context3(dec_hw);
    decCodecCtx_hw->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // decCodecCtx_hw->pix_fmt = AV_PIX_FMT_YUV420P;
    // decCodecCtx_hw->thread_count = 0;
    decCodecCtx_sw = avcodec_alloc_context3(dec_sw);
    decCodecCtx_sw->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // decCodecCtx_sw->pix_fmt = AV_PIX_FMT_YUV420P;
    // decCodecCtx_sw->thread_count = 0;
    if (!initHWDevice(decCodecCtx_hw, dec_hw)) {
        bHardwareEnable = false;
    }
    int32_t ret = 0;
    if ((ret = avcodec_open2(decCodecCtx_hw, dec_hw, NULL)) < 0) {
        err = "failed to open harware avcodec";
        return false;
    }
    if ((ret = avcodec_open2(decCodecCtx_sw, dec_sw, NULL)) < 0) {
        err = "failed to open software avcodec";
        return false;
    }
    bHaveInit = true;
    bHardwareEnable = true;
    return true;
}

void videoDecoder::uinitDecoder() {
    if (!bHaveInit) return;

    // Close the codec
    if (decCodecCtx_hw) {
        avcodec_free_context(&decCodecCtx_hw);
        avcodec_close(decCodecCtx_hw);
        av_free(decCodecCtx_hw);
        decCodecCtx_hw = NULL;
    }
    if (decCodecCtx_sw) {
        avcodec_free_context(&decCodecCtx_sw);
        avcodec_close(decCodecCtx_sw);
        av_free(decCodecCtx_sw);
        decCodecCtx_sw = NULL;
    }
    av_buffer_unref(&hwDevCtx);
    av_packet_unref(packet);
    av_packet_free(&packet);
    bHaveInit = false;
    hwDevCtx = NULL;
}