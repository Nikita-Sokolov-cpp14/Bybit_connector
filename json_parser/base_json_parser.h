#pragma once
#include <string>
#include <cstdint>
#include <iostream>
#include <charconv>
#include <string_view>

#include "data_structures/orderbook.h"
#include "data_structures/common_data.h"

static const size_t maxTypeStrLen = 60;

enum TypeMessage {
    TypeMessage_Unknown = 0,
    TypeMessage_Status,
    TypeMessage_Orderbook,
    TypeMessage_PublicTrade
};

TypeMessage parseTypeMessage(std::string_view str);

std::string_view getFieldValue(std::string_view field, std::string_view source, size_t pos = 0);

Side parseSide(std::string_view sideStr);

class BaseJsonParser {
public:
    void setString(std::string_view str);

    virtual void parse() = 0;

    virtual void printData() = 0;

protected:
    std::string_view string_;

    std::pair<double, double> parsePair(std::string_view pairStr);

    template<typename T>
    T convertTo(std::string_view valueStr) {
        T value;
        auto result = std::from_chars(valueStr.data(), valueStr.data() + valueStr.size(), value);
        if (result.ec != std::errc()) {
            std::cout << "Conversion failed for type " << typeid(T).name() << std::endl;
            value = T {};
        }
        return value;
    }

    template<typename MapType>
    void updateMap(MapType &map, double key, double value) {
        if (value == 0) {
            map.erase(key);
        } else {
            map[key] = value;
        }
    }

    double convertToDouble(std::string_view valueStr);

    bool convertToBool(std::string_view valueStr);
};
