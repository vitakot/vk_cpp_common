/**
Json Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_JSON_UTILS_H
#define INCLUDE_VK_TOOLS_JSON_UTILS_H

#include <nlohmann/json.hpp>
#include <magic_enum.hpp>

namespace vk {
/**
 * Helper for reading a value from nlohmann::json object.
 * @tparam ValueType
 * @param json
 * @param key
 * @param value
 * @param canThrow Function will throw an exception instead of silently ignoring a missing attribute
 * @return true if succeeded and canThrow parameter is false
 */
template <typename ValueType>
bool readValue(const nlohmann::json& json, const std::string& key, ValueType& value, const bool canThrow = false) {
    const auto it = json.find(key);

    if (canThrow) {
        if (!it.value().is_null()) {
            value = it.value();
            return true;
        }
    }
    if (it != json.end()) {
        if (!it.value().is_null()) {
            value = it.value();
            return true;
        }
    }
    return false;
}

/**
 * Helper for reading a string value from nlohmann::json object and transform it to double.
 * @param json
 * @param key
 * @param defaultVal Will be used if value cannot be found or of it is not transformable into double.
 * @return
 */
inline double readStringAsDouble(const nlohmann::json& json, const std::string& key, const double defaultVal = 0.0) {
    const auto it = json.find(key);

    try {
        if (it != json.end()) {
            if (!it.value().is_null() && it.value().is_string()) {
                return std::stod(it->get<std::string>());
            }
        }
    }
    catch (std::invalid_argument&) {
    }
    catch (std::out_of_range&) {
    }

    return defaultVal;
}

/**
 * Helper for reading a string value from nlohmann::json object and transform it to Integer.
 * @param json
 * @param key
 * @param defaultVal
 * @return
 */
inline int readStringAsInt(const nlohmann::json& json, const std::string& key, const int defaultVal = 0) {
    const auto it = json.find(key);

    try {
        if (it != json.end()) {
            if (!it.value().is_null() && it.value().is_string()) {
                return std::stoi(it->get<std::string>());
            }
        }
    }
    catch (std::invalid_argument&) {
    }
    catch (std::out_of_range&) {
    }

    return defaultVal;
}

/**
 * Helper for reading a string value from nlohmann::json object and transform it to 64b Integer.
 * @param json
 * @param key
 * @param defaultVal
 * @return
 */
inline int64_t readStringAsInt64(const nlohmann::json& json, const std::string& key, const int64_t defaultVal = 0) {
    const auto it = json.find(key);

    try {
        if (it != json.end()) {
            if (!it.value().is_null() && it.value().is_string()) {
                return std::stoll(it->get<std::string>());
            }
        }
    }
    catch (std::invalid_argument&) {
    }
    catch (std::out_of_range&) {
    }

    return defaultVal;
}

/**
 * Helper for reading a Better Enum value (http://github.com/aantron/better-enums) from nlohmann::json object.
 * @tparam ValueType
 * @param json
 * @param key
 * @param value
 * @param canThrow Function will throw an exception instead of silently ignoring a missing attribute
 * @return true if succeeded and canThrow parameter is false
 */
template <typename ValueType>
bool readEnum(const nlohmann::json& json, const std::string& key, ValueType& value, const bool canThrow = false) {
    const auto it = json.find(key);

    if (canThrow) {
        if (!it.value().is_null()) {
            value = ValueType::_from_string_nocase(it->get<std::string>().c_str());
            return true;
        }
    }
    if (it != json.end()) {
        if (!it.value().is_null() && !it->get<std::string>().empty()) {
            value = ValueType::_from_string_nocase(it->get<std::string>().c_str());
            return true;
        }
    }
    return false;
}

/**
 * Helper for reading a Better Enum value (https://github.com/Neargye/magic_enum) from nlohmann::json object.
 * @tparam ValueType
 * @param json
 * @param key
 * @param value
 * @param canThrow Function will throw an exception instead of silently ignoring a missing attribute
 * @return true if succeeded and canThrow parameter is false
 */
template <typename ValueType>
bool readMagicEnum(const nlohmann::json& json, const std::string& key, ValueType& value, const bool canThrow = false) {
    const auto it = json.find(key);

    if (canThrow) {
        if (!it.value().is_null()) {
            const auto v = magic_enum::enum_cast<ValueType>(it->get<std::string>(), magic_enum::case_insensitive);
            if (v) {
                value = *v;
                return true;
            }
        }
    }
    else {
        if (it != json.end()) {
            if (!it.value().is_null() && !it->get<std::string>().empty()) {
                const auto v = magic_enum::enum_cast<ValueType>(it->get<std::string>(), magic_enum::case_insensitive);
                if (v) {
                    value = *v;
                    return true;
                }
            }
        }
    }
    return false;
}

inline std::string queryStringFromJson(const nlohmann::json& pars) {
    std::string queryStr;

    for (const auto& el : pars.items()) {
        queryStr.append(el.key());
        queryStr.append("=");
        std::string val = el.value().dump();
        val.erase(std::ranges::remove(val, '\"').begin(), val.end());
        queryStr.append(val);
        queryStr.append("&");
    }
    if (!queryStr.empty()) {
        queryStr.pop_back();
    }

    return queryStr;
}
}

#endif // INCLUDE_VK_TOOLS_JSON_UTILS_H
