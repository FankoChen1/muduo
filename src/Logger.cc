#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

#include "Logger.h"
#include "Timestamp.h"

Logger::Logger()
{
    openLogFiles();
}

void Logger::openLogFiles()
{
    // 获取当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        std::cerr << "getcwd error: " << strerror(errno) << std::endl;
        return;
    }

    // 获取上级目录
    std::string currentDir(cwd);
    size_t pos = currentDir.find_last_of('/');
    std::string parentDir = (pos != std::string::npos) ? currentDir.substr(0, pos) : currentDir;
    std::string logDir = parentDir + "/log";

    // 创建log目录（如果不存在）
    struct stat st;
    if (stat(logDir.c_str(), &st) != 0) {
        if (mkdir(logDir.c_str(), 0755) != 0) {
            std::cerr << "mkdir error: " << strerror(errno) << std::endl;
            return;
        }
    }

    // 打开不同级别的日志文件
    logFiles_[INFO]  = std::unique_ptr<std::ofstream>(new std::ofstream(logDir + "/info.log", std::ios::app));
    logFiles_[ERROR] = std::unique_ptr<std::ofstream>(new std::ofstream(logDir + "/error.log", std::ios::app));
    logFiles_[FATAL] = std::unique_ptr<std::ofstream>(new std::ofstream(logDir + "/fatal.log", std::ios::app));
    logFiles_[DEBUG] = std::unique_ptr<std::ofstream>(new std::ofstream(logDir + "/debug.log", std::ios::app));
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

void Logger::log(std::string msg)
{
    std::string pre = "";
    switch(logLevel_)
    {
    case INFO:
        pre = "[INFO]";
        break;
    case ERROR:
        pre = "[ERROR]";
        break;
    case FATAL:
        pre = "[FATAL]";
        break;
    case DEBUG:
        pre = "[DEBUG]";
        break;
    default:
        break;
    }
    std::string logLine = pre + Timestamp::now().toString() + " : " + msg + "\n";
    auto it = logFiles_.find(logLevel_);
    if (it != logFiles_.end() && it->second && it->second->is_open())
    {
        *(it->second) << logLine;
        it->second->flush();
    }
    // 也可以选择是否继续输出到控制台
    // std::cout << logLine;
}