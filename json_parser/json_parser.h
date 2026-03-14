#pragma once
#include <string>
#include <cstdint>
#include <iostream>
#include <charconv>
#include <string_view>

#include "order_book/order_book.h"

enum TypeMessage {
    TypeMessage_Unknown = 0,
    TypeMessage_Status,
    TypeMessage_Snapshot,
    TypeMessage_Delta
};

struct Message {
    std::string_view topic;
    TypeMessage typeMessage;
    uint64_t ts;
    uint64_t cts;
};

struct StatusMessage {
    bool sucsess;
    std::string_view retMessage;
    std::string_view conId;
    std::string_view reqId;
    std::string_view operation;

    StatusMessage();
};

class JsonParser {
public:
    void setString(std::string_view str);

    void parseTypeMessage();

    std::string_view getFieldValue(std::string_view field, std::string_view source);

    void parse();

    void parseStatusMessage();

    void parseDataMessage();

    void printData();

    void printStatus();

protected:
    enum TypeArray {
        TypeArray_Bids = 0,
        TypeArray_Asks
    };

    std::string_view string_;

    std::string_view topic_;
    TypeMessage typeMessage_;
    uint64_t ts_;
    std::string_view s_;
    boost::container::flat_map<double, double, std::greater<double> > levelsBids_;
    boost::container::flat_map<double, double, std::greater<double> > levelsAsks_;
    uint64_t u_;
    uint64_t seq_;
    uint64_t cts_;

    StatusMessage statusMessage_;

    void parseDataSection(std::string_view dataStr);

    void parseArray(std::string_view arrayStr, const TypeArray &typeArray);

    std::pair<double, double> parsePair(std::string_view pairStr);

    template<typename T>
    void convertTo(std::string_view valueStr, T &value) {
        auto result = std::from_chars(valueStr.data(), valueStr.data() + valueStr.size(), value);

        if (result.ec != std::errc()) {
            std::cout << "Conversion failed for type " << typeid(T).name() << std::endl;
            value = T {};
        }
    }
};
