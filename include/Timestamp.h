#pragma once

#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    static Timestamp invalid()
    {
        return Timestamp();
    }

    bool valid() const { return microSecondsSinceEpoch_ > 0; }

    static const int kMicroSecondsPerSecond = 1000 * 1000;
private:
    int64_t microSecondsSinceEpoch_;
};

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

inline bool operator<(const Timestamp& lhs, const Timestamp& rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}