/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2024-01-11 14:22:51
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-16 09:16:26
 */
#include <stdio.h>
#include <stdlib.h>

#include "LiveLogger.hh"
#include "liveRtspServer.h"
#include "videoRead.h"

static std::shared_ptr<LiveRtspServer> pServer;
void videoReadCallback(unsigned char *buf, int len) {
    // LIVE_LOG(INFO) << "videoReadCallback len:" << len;
    if (pServer.get() != NULL) {
        pServer->sendVideoData(buf, len);
    }
}

int main(int argc, char *argv[]) {
    char filePath[256] = {0};
    if (argc < 2) {
        strcpy(filePath, "/home/user/work/video/hkcamera-1080p25-4mb.h264");
    } else {
        strcpy(filePath, argv[1]);
    }
    pServer = LiveRtspServer::construct("h264", "none", 60, 8554, "stream1");
    LIVE_LOG(INFO) << "rtsp url:" << pServer->RtspUrl();
    pServer->start();
    VideoRead videoRead(filePath, videoReadCallback, true);
    videoRead.start();
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}