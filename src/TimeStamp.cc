#include <sys/time.h>
#include "Timestamp.h"

Timestamp::Timestamp() : microSecondsSinceEpoch_(0)
{
}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return Timestamp(tv.tv_sec * kMicroSecondsPerSecond + tv.tv_usec);
}

std::string Timestamp::toString() const
{
    char buf[128];
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    tm *time_t = localtime(&seconds);
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d.%06ld",
        time_t->tm_year + 1900,
        time_t->tm_mon + 1,
        time_t->tm_mday,
        time_t->tm_hour,
        time_t->tm_min,
        time_t->tm_sec,
        microseconds);
    return buf;
}
