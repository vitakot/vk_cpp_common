/**
Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef UTILS_H
#define UTILS_H

#include <chrono>
#include <string>
#include <fmt/core.h>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <cstdint>

namespace vk {
typedef double DATE;

typedef struct T6 {
    double time; // GMT timestamp of fClose
    float fHigh, fLow; // (f1,f2)
    float fOpen, fClose; // (f3,f4)
    float fVal, fVol; // additional data, f.i. spread and volume (f5,f6)
} T6; // 6-stream tick, .t6 file content

inline DATE convertTimeMs(const std::int64_t t64) {
    if (t64 == 0) return 0.;
    return (25569. + static_cast<double>(t64 / 1000) / (24. * 60. * 60.));
}

#if defined __linux__
inline __int64_t convertTimeMs(const DATE Date) {
    return 1000 * static_cast<__int64_t>((Date - 25569.) * 24. * 60. * 60.);
}
#else
inline __int64 convertTimeMs(DATE Date) {
    return 1000 * (__int64) ((Date - 25569.) * 24. * 60. * 60.);
}
#endif

inline DATE convertTimeS(const std::int64_t t64) {
    if (t64 == 0) return 0.;
    return (25569. + static_cast<double>(t64) / (24. * 60. * 60.));
}

inline std::int64_t convertTimeS(const DATE Date) {
    return static_cast<std::int64_t>((Date - 25569.) * 24. * 60. * 60.);
}

using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

constexpr char hexMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

inline TimePoint currentTime() {
    return Clock::now();
}

inline std::chrono::milliseconds getMsTimestamp(const TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch());
}

inline std::string stringToHex(const unsigned char* data, const std::size_t len) {
    std::string s(len * 2, ' ');
    for (std::size_t i = 0; i < len; ++i)
    {
        s[2 * i] = hexMap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexMap[data[i] & 0x0F];
    }
    return s;
}

/**
 * Pack Date and Time components into MS COM time format (a double)
 * @param year
 * @param month
 * @param day
 * @param hour
 * @param min
 * @param sec
 * @param msec
 * @return time represented by double (aka Variant in MS COM)
 */
double systemTimeToVariantTimeMs(unsigned short year, unsigned short month, unsigned short day,
                                 unsigned short hour, unsigned short min, unsigned short sec,
                                 unsigned int msec);

/**
 * Copy string src to buffer dst of size dsize.  At most dsize-1
 * chars will be copied.  Always NUL terminates (unless dsize == 0).
 * Returns strlen(src); if retval >= dsize, truncation occurred.
 */
size_t strlcpy(char* dst, const char* src, size_t dsize);

/**
 * Split string parts separated by a delimiter into a vector of sub-strings
 * @param s
 * @param delim
 * @return vector of sub-strings
 */
std::vector<std::string> splitString(const std::string& s, char delim);

/**
 * Read a string and represents it as a bool if possible
 * @param v e.g. "false", "true", "1", "0"
 * @return
 */
inline bool string2bool(std::string v) {
    std::ranges::transform(v, v.begin(), ::tolower);
    return !v.empty() && (v == "true" || atoi(v.c_str()) != 0);
}

/**
 * Same as std::mktime but does not convert into local time, uses UTC instead
 * @param ptm
 * @return
 */
time_t mkgmtime(const struct tm* ptm);

/**
 * A helper for converting date-time strings into the Unix timestamp (seconds from epoch)
 * @param timeString e.g. "2022-01-28T21:45:00+00:00"
 * @param format e.g. "%Y-%m-%dT%H:%M:%S:%z"
 * @return seconds from epoch
 */
int64_t getTimeStampFromString(const std::string& timeString, const std::string& format);

/**
 * A helper for converting date-time strings into the Unix timestamp (seconds from epoch)
 * @param timeString e.g. "2022-01-28T21:45:00+00:00"
 * @param format e.g. "%Y-%m-%dT%H:%M:%S:%z"
 * @return seconds from epoch
 */
int64_t getTimeStampFromStringWithZone(const std::string& timeString, const std::string& format);

/**
 * A helper for converting date-time strings into the tm structure
 * @param timeString e.g. "2022-01-28T21:45:00+00:00"
 * @param format e.g. "%Y-%m-%dT%H:%M:%S:%z"
 * @return filled tm structure
 */
std::tm getTimeFromString(const std::string& timeString, const std::string& format);

/**
 * A helper for converting Unix timestamp (seconds from epoch) into date-time strings
 * @param timeStamp
 * @param format
 * @return
 */
std::string getDateTimeStringFromTimeStamp(int64_t timeStamp, const std::string& format, bool isMs = false);

/**
 * Convert double into string with given precision
 * @param precision
 * @param quantity
 * @return
 */
std::string formatDouble(int64_t precision, double val);

/**
 * The type-safe C++ sign function
 * @tparam T
 * @param val
 * @return -1, 1 or 0
 */
template <typename T>
int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

/**
* Template for using dynamically created strings as arguments to fmt::format function
*/

template <typename... Args>
std::string dyna_print(std::string_view rt_fmt_str, Args&&... args) {
    return fmt::vformat(rt_fmt_str, fmt::make_format_args(args...));
}

namespace noncopyable_ {
// protection from unintended Argument-dependent lookup

class noncopyable {
protected:
#if !defined(NO_CXX11)

    constexpr noncopyable() = default;

    ~noncopyable() = default;

#else
    noncopyable() {}
    ~noncopyable() {}
#endif
#if !defined(NO_CXX11)

    noncopyable(const noncopyable&) = delete;

    noncopyable& operator=(const noncopyable&) = delete;

#else
    private:
    noncopyable(const noncopyable &);
    noncopyable &operator=(const noncopyable &);
#endif
};
}

typedef noncopyable_::noncopyable noncopyable;

std::string getHomeDir();

bool strCmpCaseIns(const std::string& a, const std::string& b);

std::string queryStringFromMap(const std::map<std::string, std::string>& v);

void replaceAll(std::string& s, const std::string& search, const std::string& replace);

std::error_code createDirectoryRecursively(const std::string& dirName);

std::vector<std::filesystem::path> findFilePaths(const std::string& dirPath, const std::string& extension);
}
#endif //UTILS_H
