#pragma once

#include <string>
#include <cstdint>
#include <iostream>
#include <charconv>
#include <string_view>

#include "base_json_parser.h"
#include "data_structures/public_trade.h"

class PublicTradeJsonParser : public BaseJsonParser {
public:
    PublicTradeJsonParser(PublicTrade *const publicTrade);

    void parse() override;

    void printData() override;

private:
    PublicTrade *const publicTrade_;
};
