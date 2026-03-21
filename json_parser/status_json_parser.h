#pragma once
#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/status_message.h"

class StatusJsonParser : public BaseJsonParser {
public:
    StatusJsonParser(StatusMessage *const statusMessage);

    void parse() override;

    void printData() override;

private:
    StatusMessage *const statusMessage_;
};
