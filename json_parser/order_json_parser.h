#pragma once
#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/order.h"

class OrderJsonParser : public BaseJsonParser {
public:
    OrderJsonParser(OrderHFT *const orderHFT);

    void parse() override;

    void parseData(std::string_view dataStr);

    void printData() override;

private:
    OrderHFT *const orderHFT_;
};
