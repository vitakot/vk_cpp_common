/**
Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/utils/utils.h"
#include "date.h"
#include <fmt/format.h>
#include <iomanip>
#include <map>
#include <filesystem>
#include <regex>
#include <sstream>

namespace vk {
static constexpr int SECONDS_PER_MINUTE = 60;
static constexpr int SECONDS_PER_HOUR = 3600;
static constexpr int SECONDS_PER_DAY = 86400;
static const int DAYS_OF_MONTH[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

double systemTimeToVariantTimeMs(const unsigned short year, const unsigned short month, const unsigned short day,
                                 const unsigned short hour, const unsigned short min, const unsigned short sec,
                                 const unsigned int msec) {
    int m12 = (month - 14) / 12;
    double dateVal =
        /* Convert Day/Month/Year to a Julian date - from PostgreSQL */
        (1461 * (year + 4800 + m12)) / 4 + (367 * (month - 2 - 12 * m12)) / 12 -
        (3 * ((year + 4900 + m12) / 100)) / 4 + day - 32075
        - 1757585 /* Convert to + days from 1 Jan 100 AD */
        - 657434; /* Convert to +/- days from 1 Jan 1899 AD */
    double dateSign = (dateVal < 0.0) ? -1.0 : 1.0;
    dateVal += dateSign * (msec + sec * 1000 + min * 60000 + hour * 3600000) / 86400000.0;
    return dateVal;
}

size_t strlcpy(char* dst, const char* src, size_t dsize) {
    const char* osrc = src;
    size_t nleft = dsize;

    /* Copy as many bytes as will fit. */
    if (nleft != 0) {
        while (--nleft != 0) {
            if ((*dst++ = *src++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src. */
    if (nleft == 0) {
        if (dsize != 0)
            *dst = '\0'; /* NUL-terminate dst */
        while (*src++);
    }

    return (src - osrc - 1); /* count does not include NUL */
}

std::vector<std::string> splitString(const std::string& s, char delim) {
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;

    while (std::getline(ss, item, delim)) {
        elems.push_back(std::move(item));
    }

    return elems;
}

inline bool isLeapYear(short year) {
    if (year % 4 != 0) return false;
    if (year % 100 != 0) return true;
    return (year % 400) == 0;
}

time_t mkgmtime(const struct tm* ptm) {
    time_t secs = 0;
    // tm_year is years since 1900
    int year = ptm->tm_year + 1900;
    for (int y = 1970; y < year; ++y) {
        secs += (isLeapYear(y) ? 366 : 365) * SECONDS_PER_DAY;
    }
    // tm_mon is month from 0..11
    for (int m = 0; m < ptm->tm_mon; ++m) {
        secs += DAYS_OF_MONTH[m] * SECONDS_PER_DAY;
        if (m == 1 && isLeapYear(static_cast<short>(year))) {
            secs += SECONDS_PER_DAY;
        }
    }
    secs += (ptm->tm_mday - 1) * SECONDS_PER_DAY;
    secs += ptm->tm_hour * SECONDS_PER_HOUR;
    secs += ptm->tm_min * SECONDS_PER_MINUTE;
    secs += ptm->tm_sec;

    return secs;
}

int64_t getTimeStampFromString(const std::string& timeString, const std::string& format) {
    std::tm time{};
    std::istringstream ss(timeString);
    ss >> std::get_time(&time, format.c_str());
    return mkgmtime(&time);
}

int64_t getTimeStampFromStringWithZone(const std::string& timeString, const std::string& format) {
    std::istringstream ss(timeString);
    std::chrono::sys_seconds dt;
    ss >> date::parse(format, dt);
    return dt.time_since_epoch().count();
}

std::tm getTimeFromString(const std::string& timeString, const std::string& format) {
    std::tm time{};
    std::istringstream ss(timeString);
    ss >> std::get_time(&time, format.c_str());
    return time;
}

std::string getDateTimeStringFromTimeStamp(int64_t timeStamp, const std::string& format, bool isMs) {
    std::string retVal;
    std::time_t secsSinceEpoch;
    std::time_t ms;

    if (!isMs) {
        secsSinceEpoch = timeStamp;
    }
    else {
        secsSinceEpoch = timeStamp / 1000;
        ms = timeStamp % 1000;
    }

    auto timeStruct = std::gmtime(&secsSinceEpoch);
    char timeString[128];
    std::strftime(timeString, 128, format.c_str(), timeStruct);
    retVal.append(timeString);

    if (isMs) {
        std::string msString(timeString);
        msString.append(".");
        msString.append(std::to_string(ms));
        retVal = msString;
    }

    return retVal;
}

std::string formatDouble(int64_t precision, const double val) {
    std::string format = "{:.";
    format.append(std::to_string(precision));
    format.append("f}");
    return dyna_print(format, val);
}

std::string getHomeDir() {
    std::string retVal;

#ifdef linux
    retVal = std::string(getenv("HOME"));
#endif

#ifdef _WIN32
    char *homePath;
    char *homeDrive;
    size_t len;
    errno_t err = _dupenv_s(&homePath, &len, "HOMEPATH");
    err = _dupenv_s(&homeDrive, &len, "HOMEDRIVE");
    retVal = std::string(homeDrive) + std::string(homePath);
#endif

    return retVal;
}

bool strCmpCaseIns(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                          return tolower(a) == tolower(b);
                      });
}

std::string queryStringFromMap(const std::map<std::string, std::string>& v) {
    std::string queryStr;

    for (const auto& parameter : v) {
        queryStr.append(parameter.first);
        queryStr.append("=");
        queryStr.append(parameter.second);
        queryStr.append("&");
    }

    if (!queryStr.empty()) {
        queryStr.pop_back();
    }

    return queryStr;
}

void replaceAll(std::string& s, const std::string& search, const std::string& replace) {
    for (size_t pos = 0;; pos += replace.length()) {
        pos = s.find(search, pos);

        if (pos == std::string::npos)
            break;

        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

std::error_code createDirectoryRecursively(const std::string& dirName) {
    std::error_code err;
    if (!std::filesystem::create_directories(dirName, err)) {
        if (std::filesystem::exists(dirName)) {
            return {};
        }
        return err;
    }
    return {};
}

std::vector<std::filesystem::path> findFilePaths(const std::string& dirPath, const std::string& extension) {
    std::vector<std::filesystem::path> retVal;
    std::regex dataFileFilter(extension);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (!std::filesystem::is_regular_file(entry.status()))
            continue;

        if (!std::regex_match(entry.path().extension().string(), dataFileFilter))
            continue;

        retVal.push_back(entry);
    }

    return retVal;
}
}
