#include "public_trade_json_parser.h"

PublicTradeJsonParser::PublicTradeJsonParser(PublicTrade *const publicTrade) :
publicTrade_(publicTrade) {
}

void PublicTradeJsonParser::parse() {
    if (publicTrade_ == nullptr) {
        std::cout << "PublicTradeJsonParser::parse: ERROR: publicTrade_ = nulptr" << std::endl;
        return;
    }

    std::cout << "in public trade parser" << std::endl;
}

void PublicTradeJsonParser::printData() {
}
