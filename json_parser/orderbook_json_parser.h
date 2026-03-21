#pragma once
#include <string>
#include <cstdint>
#include <iostream>
#include <charconv>
#include <string_view>

#include "base_json_parser.h"
#include "data_structures/orderbook.h"

class OrderBookJsonParser : public BaseJsonParser {
public:
    OrderBookJsonParser(OrderBook *const orderBook);

    void parse() override;

    void printData() override;

private:
    enum TypeArray {
        TypeArray_Unknown = 0,
        TypeArray_Bids,
        TypeArray_Asks
     };
    enum TypeOrderbookMessage {
        TypeOrderbookMessage_Unkown = 0,
        TypeOrderbookMessage_Delta,
        TypeOrderbookMessage_Snapshot
    };

    OrderBook *const orderBook_;
    TypeOrderbookMessage typeOrderbookMessage_;

    void parseDataSection(std::string_view dataStr);

    void parseArray(std::string_view arrayStr, const TypeArray &typeArray);

    void parseDataMessage();

    void parseTypeOrderbookMessage();
};
