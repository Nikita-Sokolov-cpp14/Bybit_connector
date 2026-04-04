#pragma once
#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/execution_fast.h"

class ExecutionFastJsonParser : public BaseJsonParser {
public:
    ExecutionFastJsonParser(ExecutionFast *const executionFastconst);

    void parse() override;

    void parseData(std::string_view dataStr);

    void printData() override;

private:
    ExecutionFast *const executionFast_;
};
