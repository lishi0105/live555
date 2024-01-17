/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2024-01-12 13:51:44
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-16 15:33:44
 */
#ifndef _VIDEO_READ_HH_
#define _VIDEO_READ_HH_
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

typedef struct _FrameInfo{
    unsigned ipos;
    unsigned isize;
    unsigned itype;
    bool bKeyFrame;
} fileFrameInfo;

typedef std::function<void(unsigned char *buf, int len)> VideoReadCallback;

class VideoRead {
public:
    VideoRead(const char *path, VideoReadCallback cb, bool loop = false);
    ~VideoRead();
    void start();
    void stop();

private:
    void release();
    void readThread();

private:
    bool bStart;
    bool bloop_;
    bool bExit;
    double fps_;
    std::string readFilePath;
    std::vector<fileFrameInfo> fileInfoVec; 
    int videoStreamIndex;
    std::thread readThreadTid;
    VideoReadCallback cb_;
    AVFormatContext *formatContext;
};

#endif  // !_VIDEO_READ_HH_