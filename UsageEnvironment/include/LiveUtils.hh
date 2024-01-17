/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-18 14:17:18
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-11 17:05:19
 */
#include <stdint.h>
#include <string.h>

#include <iostream>
#ifdef __linux__
#include <strings.h>
#endif
class LiveUtils {
public:
    typedef enum TimeLevel_ {
        LEVEL_SEC = 0x0,
        LEVEL_MILLSEC = 0x01,
        LEVEL_MICROSEC = 0x02
    } TimeLevel;
    static uint64_t getTickCount_(TimeLevel t = LEVEL_MILLSEC);
    static void outputHexBuf(std::ostream &os, void *buff, uint32_t count,
                             bool sizeOut = false);
    static std::string uint8ToHexStr(uint8_t *buff, uint32_t count,
                                     bool upper = true);
    static void toUpperCase(std::string &str);
    static bool ParseUdpUrl(const std::string &strURL, std::string &strIP,
                            int &nPort);
    static bool parsingRTSPURL(const std::string &url, std::string &username,
                               std::string &password,
                               std::string &UnauthorizedPath);
    static bool compareNoCase(const std::string &str1, const std::string &str2);
    static const uint32_t u32MaxValue = 0xFFFFFFFF;
};