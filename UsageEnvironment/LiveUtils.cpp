/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-18 14:17:28
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-11 17:04:57
 */
#include "LiveUtils.hh"

#include <string.h>
#include <sys/time.h>
#include <time.h>

uint64_t LiveUtils::getTickCount_(TimeLevel t) {
    struct timeval tv;
    uint64_t tickCount = 0;
    if (gettimeofday(&tv, NULL) != -1) {
        return 0;
    }
    if (t == LEVEL_SEC) {
        tickCount = tv.tv_sec;
    } else if (t == LEVEL_MILLSEC) {
        tickCount = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    } else {
        tickCount = tv.tv_sec * 1000000 + tv.tv_usec;
    }
    return tickCount;
}

void LiveUtils::outputHexBuf(std::ostream &os, void *buff, uint32_t count,
                             bool sizeOut) {
    if (count <= 0 || buff == nullptr) {
        os << "nullptr" << std::endl;
        return;
    }

    if (sizeOut) {
        os << count << " ";
    }
    uint8_t *frame = (uint8_t *)buff;
    char *buf = new char[count * 3 + 1];
    if (buf != nullptr) {
        memset(buf, 0, count * 3 + 1);
        for (uint32_t i = 0; i < count; ++i) {
            snprintf(buf + i * 3, 4, "%02x ", (frame[i] & 0xff));
        }
        os << buf;
        delete[] buf;
        buf = nullptr;
    }
    os << std::endl;
}

std::string LiveUtils::uint8ToHexStr(uint8_t *buff, uint32_t count,
                                     bool upper) {
    std::string str;
    if (count <= 0 || buff == nullptr) {
        return "nullptr";
    }

    uint8_t *frame = (uint8_t *)buff;
    char *buf = new char[count * 3 + 1];
    if (buf != nullptr) {
        memset(buf, 0, count * 3 + 1);
        for (uint32_t i = 0; i < count; ++i) {
            if (upper) {
                snprintf(buf + i * 3, 4, "%02X ", (frame[i] & 0xff));
            } else {
                snprintf(buf + i * 3, 4, "%02x ", (frame[i] & 0xff));
            }
        }
        str = buf;
        delete[] buf;
        buf = nullptr;
    }
    return str;
}

void LiveUtils::toUpperCase(std::string &str) {
    for (char &c : str) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
}

bool LiveUtils::ParseUdpUrl(const std::string &strURL_, std::string &strIP,
                            int &nPort) {
    if (strURL_.empty()) return false;
    // 小写转大写
    std::string strURL = strURL_;
    toUpperCase(strURL);

    if (strURL.find("UDP://@") != std::string::npos) {
        std::string strIpAndPort = strURL.substr(strlen("UDP://@"));

        int nPos = strIpAndPort.find(":");
        strIP = strIpAndPort.substr(0, nPos);
        nPort = atoi(strIpAndPort.substr(nPos + 1).c_str());
    } else if (strURL.find("UDP://") != std::string::npos) {
        std::string strIpAndPort = strURL.substr(strlen("UDP://"));

        int nPos = strIpAndPort.find(":");
        strIP = strIpAndPort.substr(0, nPos);
        nPort = atoi(strIpAndPort.substr(nPos + 1).c_str());
    } else {
        std::cout << "Illegal URL! \n" << std::endl;
        return false;
    }
    return true;
}

bool LiveUtils::parsingRTSPURL(const std::string &url_, std::string &username_,
                               std::string &password_,
                               std::string &UnauthorizedPath_) {
    //解析rtsp连接的用户名与密码
    if (url_.empty()) {
        std::cout << "url is empty" << std::endl;
        return false;
    }
    if (url_.length() > 128) {
        std::cout << "url is too long" << std::endl;
        return false;
    }
    char url[128] = {0};
    char *username = NULL;
    char *password = NULL;
    char *UnauthorizedPath = NULL;
    memcpy(url, url_.c_str(), url_.length());
    do {
        // Parse the URL as
        // "rtsp://[<username>[:<password>]@]<server-address-or-name>[:<port>][/<stream-name>]"
        char const *prefix = "rtsp://";
        unsigned const prefixLength = 7;
        //         if (_strncasecmp(rtspURL, prefix, prefixLength) != 0)
        //         {
        //             printf("URL is not of the form %s\n", prefix);
        //             break;
        //         }

        char const *from = &url[prefixLength];

        // Check whether "<username>[:<password>]@" occurs next.
        // We do this by checking whether '@' appears before the end of the URL,
        // or before the first '/'.
        char const *colonPasswordStart = NULL;
        char const *p;
        for (p = from; *p != '\0'; ++p) {
            if (*p == ':' && colonPasswordStart == NULL) {
                colonPasswordStart = p;
            } else if (*p == '@') {
                // We found <username> (and perhaps <password>).  Copy them into
                // newly-allocated result strings:
                if (colonPasswordStart == NULL) colonPasswordStart = p;

                char const *usernameStart = from;
                unsigned usernameLen = colonPasswordStart - usernameStart;
                username =
                    new char[usernameLen + 1];  // allow for the trailing '\0'
                unsigned i = 0;
                while (usernameLen > 0) {
                    int nBefore = 0;
                    int nAfter = 0;

                    if (*usernameStart == '%' && usernameLen >= 3 &&
                        sscanf(usernameStart + 1, "%n%2hhx%n", &nBefore,
                               username, &nAfter) == 1) {
                        unsigned codeSize =
                            nAfter - nBefore;  // should be 1 or 2

                        ++username;
                        usernameStart += (1 + codeSize);
                        usernameLen -= (1 + codeSize);
                    } else {
                        username[i] = *usernameStart++;
                        i++;
                        --usernameLen;
                    }
                }
                username[i] = '\0';

                char const *passwordStart = colonPasswordStart;
                if (passwordStart < p) ++passwordStart;  // skip over the ':'
                unsigned passwordLen = p - passwordStart;
                password =
                    new char[passwordLen + 1];  // allow for the trailing '\0'
                i = 0;
                while (passwordLen > 0) {
                    int nBefore = 0;
                    int nAfter = 0;

                    if (*passwordStart == '%' && passwordLen >= 3 &&
                        sscanf(passwordStart + 1, "%n%2hhx%n", &nBefore,
                               password, &nAfter) == 1) {
                        unsigned codeSize =
                            nAfter - nBefore;  // should be 1 or 2

                        ++password;
                        passwordStart += (1 + codeSize);
                        passwordLen -= (1 + codeSize);
                    } else {
                        password[i] = *passwordStart++;
                        i++;
                        --passwordLen;
                    }
                }
                password[i] = '\0';

                from = p + 1;  // skip over the '@'

                UnauthorizedPath = new char[128];

                for (i = 0; i < prefixLength; i++) {
                    UnauthorizedPath[i] = prefix[i];
                }

                for (i = 0; i < strlen(from); i++) {
                    UnauthorizedPath[7 + i] = from[i];
                }
                UnauthorizedPath[7 + i] = '\0';
            }
        }
    } while (0);
    if (username == NULL || password == NULL || UnauthorizedPath == NULL) {
        std::cout << "parsingRTSPURL failed" << std::endl;
        if (username != NULL) {
            delete[] username;
            username = NULL;
        }
        if (password != NULL) {
            delete[] password;
            password = NULL;
        }
        if (UnauthorizedPath != NULL) {
            delete[] UnauthorizedPath;
            UnauthorizedPath = NULL;
        }
        return false;
    }
    username_ = username;
    password_ = password;
    UnauthorizedPath_ = UnauthorizedPath;
    delete[] username;
    username = NULL;
    delete[] password;
    password = NULL;
    delete[] UnauthorizedPath;
    UnauthorizedPath = NULL;
    return true;
}

bool LiveUtils::compareNoCase(const std::string &str1,
                              const std::string &str2) {
    std::string str1_ = str1;
    std::string str2_ = str2;
#ifdef _WIN32
    return stricmp(str1_.c_str(), str2_.c_str()) == 0;
#elif __linux__
    return strcasecmp(str1_.c_str(), str2_.c_str()) == 0;
#else
    toUpperCase(str1_);
    toUpperCase(str2_);
    return str1_ == str2_;
#endif
}