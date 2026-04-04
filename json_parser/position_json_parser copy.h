#pragma once
#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/position.h"

class PositionJsonParser : public BaseJsonParser {
public:
    PositionJsonParser(PositionHFT *const position);

    void parse() override;

    void parseData(std::string_view dataStr);

    void printData() override;

private:
    PositionHFT *const position_;
};
