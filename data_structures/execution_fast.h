#pragma once

#include <cstdint>
#include <string_view>
#include "common_data.h"

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
