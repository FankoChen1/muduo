#pragma once

#include <string>
#include <fstream>
#include <map>
#include <memory>

#include "noncopyable.h"

#define LOG_INFO(logmsgFormat, ...)         \
    do                                      \
    {                                       \
        Logger &logger = Logger::instance();\
        logger.setLogLevel(INFO);           \
        char buf[1024] = {0};               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);                    \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)        \
    do                                      \
    {                                       \
        Logger &logger = Logger::instance();\
        logger.setLogLevel(ERROR);          \
        char buf[1024] = {0};               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);                    \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)         \
    do                                      \
    {                                       \
        Logger &logger = Logger::instance();\
        logger.setLogLevel(DEBUG);          \
        char buf[1024] = {0};               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);                    \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

class Logger : noncopyable
{
public:
    static Logger &instance();
    void setLogLevel(int level);
    void log(std::string msg);

private:
    Logger(); // 构造函数私有化
    void openLogFiles(); // 打开所有日志文件
    std::map<int, std::unique_ptr<std::ofstream>> logFiles_; // 日志文件流
    int logLevel_;
};