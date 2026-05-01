#pragma once
#include <string>
#include <cstdint>

enum TypeOrderOperation {
    TypeOrderOperation_Unknown = 0,
    TypeOrderOperation_Auth, // авторизация
    TypeOrderOperation_Create,
    TypeOrderOperation_Amend,
    TypeOrderOperation_Cancel
};

struct OrderOperation {
    int retCode;
    std::string_view retMsg;
    TypeOrderOperation op;
    std::string_view connId;

    std::string_view orderId;
    std::string_view orderLinkId;

        // retExtInfo

    std::string_view X_Bapi_Limit_Status;
    uint64_t X_Bapi_Limit_Reset_Timestamp;
    std::string_view Traceid;
    uint64_t Timenow;
    std::string_view X_Bapi_Limit;

    void print();
};
