#pragma once

#include <cstdint>
#include <string_view>
#include "common_data.h"

enum class Category : uint8_t {
    Unknown = 0,
    Linear = 1,    // "linear"
    Inverse = 2,   // "inverse"
    Option = 3     // "option"
};

class ExecutionFast {
public:
    std::string_view topic;
    uint64_t creationTime;
    Category category;
    std::string_view symbol;
    std::string_view execId;
    double execPrice;
    double execQty;
    std::string_view orderId;
    bool isMaker;
    std::string_view orderLinkId;
    Side side;
    uint64_t execTime;
    uint64_t seq;

    void print();
};
