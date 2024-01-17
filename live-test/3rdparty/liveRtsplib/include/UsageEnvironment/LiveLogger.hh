/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-05-17 08:37:12
 * @LastEditors: 李石
 * @LastEditTime: 2024-01-11 14:46:38
 */
#ifndef LIVE_LOGGER_H
#define LIVE_LOGGER_H

#include <string.h>
#include <sys/time.h>

#include <cstdio>
#include <iostream>
#include <mutex>
#include <sstream>

#ifndef __FILENAME__
#define __FILENAME__                                                         \
    (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 \
                                      : __FILE__)
#endif

enum LogLevel {
    INFO,
    TRACE,
    WARN,
    ERROR,
    FATAL,
};

static std::mutex log_mutex_;
class Logger {
public:
    Logger(const char* file, int line, const char* func, LogLevel level = INFO,
           bool printFunctionName = false)
        : level_(level),
          file_(file),
          func_(func),
          line_(line),
          printFunctionName_(printFunctionName),
          endline_(false) {
        switch (level_) {
            case INFO:
                color_code_ = "\033[32m";
                break;
            case TRACE:
                color_code_ = "\033[34m";
                break;
            case WARN:
                color_code_ = "\033[33m";
                break;
            case ERROR:
                color_code_ = "\033[31m";
                break;
            case FATAL:
                color_code_ = "\033[35m";
                break;
            default:
                color_code_ = "\033[0m";
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm* lt = localtime(&tv.tv_sec);
        char buf[100] = {0};
        snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour,
                 lt->tm_min, lt->tm_sec, (long)(tv.tv_usec % 1000000));
        stream_ << buf << LogLevelToString(level_) << " " << file_ << ":"
                << line_;
        stream_ << "] ";
        if (printFunctionName_) {
            stream_ << func_ << " ";
        }
    }

    ~Logger() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cout << color_code_ << stream_.str() << "\033[0m"
                  << "";
        if (!endline_) {
            std::cout << std::endl;
        }
        if (level_ == FATAL) {
            std::exit(EXIT_FAILURE);
        }
    }

    template <typename T>
    Logger& operator<<(const T& msg) {
        if (endline_) {
            stream_ << std::endl;
            endline_ = false;
        }
        stream_ << msg;
        return *this;
    }

    Logger& operator<<(std::ostream& (*os)(std::ostream&)) {
        if (os == static_cast<std::ostream& (*)(std::ostream&)>(std::endl)) {
            stream_ << os;
            endline_ = true;
        } else {
            stream_ << os;
        }
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    const char* func_;
    int line_;
    bool printFunctionName_;
    bool endline_;
    std::string color_code_;
    std::stringstream stream_;

    static std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case INFO:
                return "I";
            case TRACE:
                return "T";
            case WARN:
                return "W";
            case ERROR:
                return "E";
            case FATAL:
                return "F";
            default:
                return "U";
        }
    }
};

#ifndef LIVE_LOG
#define LIVE_LOG(...) Logger(__FILENAME__, __LINE__, __FUNCTION__, __VA_ARGS__)
#endif

#endif  // LIVE_LOGGER_H_