/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2024-01-12 13:51:52
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-16 15:59:03
 */
#include "videoRead.h"

#include <unistd.h>

#include "LiveLogger.hh"

VideoRead::VideoRead(const char *path, VideoReadCallback cb, bool loop) {
    if (path == NULL || strlen(path) == 0) {
        LIVE_LOG(ERROR) << "VideoRead::VideoRead() path is NULL";
        throw std::runtime_error("VideoRead::VideoRead() path is NULL");
    }
    if (access(path, F_OK) != 0) {
        LIVE_LOG(ERROR) << "VideoRead::VideoRead() path is not exist";
        throw std::runtime_error("VideoRead::VideoRead() path is not exist");
    }
    cb_ = cb;
    videoStreamIndex = -1;
    formatContext = NULL;
    int err_code = 0;
    if (err_code = avformat_open_input(&formatContext, path, NULL, NULL) != 0) {
        release();
        LIVE_LOG(ERROR) << "avformat_open_input " << path << " failed ";
        throw std::runtime_error(
            "VideoRead::VideoRead() avformat_open_input failed");
    }
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        release();
        LIVE_LOG(ERROR)
            << "VideoRead::VideoRead() avformat_find_stream_info failed";
        throw std::runtime_error(
            "VideoRead::VideoRead() avformat_find_stream_info failed");
    }
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type ==
            AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1) {
        release();
        LIVE_LOG(ERROR) << "VideoRead::VideoRead() not found video stream";
        throw std::runtime_error(
            "VideoRead::VideoRead() not found video stream");
    }
    AVStream *videoStream = formatContext->streams[videoStreamIndex];

    // 计算帧率
    double fps = av_q2d(videoStream->avg_frame_rate);
    fps_ = fps;
    if (fps_ <= 0) fps_ = 25;
    bloop_ = loop;
    bExit = false;
    bStart = false;
    readFilePath = path;
    readThreadTid = std::thread(&VideoRead::readThread, this);
}

VideoRead::~VideoRead() {
    stop();
    release();
}

void VideoRead::release() {
    if (formatContext != nullptr) {
        avformat_close_input(&formatContext);
        formatContext = nullptr;
    }
}

void VideoRead::readThread() {
    unsigned int pos = 0;
    unsigned timeout = 0;
    while (!bExit) {
        AVPacket packet;
        if(av_read_frame(formatContext, &packet) < 0){
            break;
        }
        fileFrameInfo info;
        info.ipos = pos;
        info.isize = packet.size;
        info.itype = packet.stream_index;
        info.bKeyFrame = packet.flags & AV_PKT_FLAG_KEY;
        fileInfoVec.push_back(info);
        timeout ++;
        if(timeout > 100){
            timeout = 0;
            usleep(1000);
        }
        pos += packet.size;
        av_packet_unref(&packet);
    }
    avformat_close_input(&formatContext);
    formatContext = nullptr;
    FILE *fp = fopen(readFilePath.c_str(), "rb");
    if (fp == NULL) {
        LIVE_LOG(ERROR) << "VideoRead::readThread() fopen failed";
        return;
    }
    LIVE_LOG(INFO)<< "VideoRead::readThread() fileInfoVec.size():" << fileInfoVec.size();
    int i = 0;
    while (!bExit) {
        if (!bStart) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        fileFrameInfo info = fileInfoVec[i];
        fseek(fp, info.ipos, SEEK_SET);
        unsigned char *buf = new unsigned char[info.isize+ 1];
        memset(buf, 0, info.isize+ 1);
        fread(buf, 1, info.isize, fp);
        if (cb_ != nullptr) {
            cb_(buf, info.isize);
        }
        delete[] buf;
        buf = NULL;
        i++;
        if(i >= fileInfoVec.size()){
            if(!bloop_){
                bExit = true;
                break;
            }
            i = 0;
            LIVE_LOG(WARN)<< "VideoRead::readThread() loop";
        }
        int duration = 1000 / (fps_ * 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    }
    fclose(fp);
    fp = NULL;
}

void VideoRead::stop() {
    if (!bStart) return;
    bStart = false;
    bExit = true;
    readThreadTid.join();
}

void VideoRead::start() {
    if (bStart) return;
    bStart = true;
}