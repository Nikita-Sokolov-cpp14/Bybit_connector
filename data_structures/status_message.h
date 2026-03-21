#pragma once

#include <string_view>

struct StatusMessage {
    bool sucsess;
    std::string_view retMessage;
    std::string_view conId;
    std::string_view reqId;
    std::string_view operation;

    StatusMessage();
};
