#include "order_operation.h"

#include <iostream>

void OrderOperation::print() {
    std::cout << " === struct OrderOperation === " << std::endl;

    std::cout << "retCode: " << retCode << std::endl;
    std::cout << "retMsg: " << retMsg << std::endl;
    std::cout << "op: " << op << std::endl;
    std::cout << "connId: " << connId << std::endl;

    std::cout << "orderId: " << orderId << std::endl;
    std::cout << "orderLinkId: " << orderLinkId << std::endl;
    std::cout << "X-Bapi-Limit-Status: " << X_Bapi_Limit_Status << std::endl;
    std::cout << "X-Bapi-Limit-Reset-Timestamp: " << X_Bapi_Limit_Reset_Timestamp << std::endl;
    std::cout << "Traceid: " << Traceid << std::endl;
    std::cout << "Timenow: " << Timenow << std::endl;
    std::cout << "X-Bapi-Limit: " << X_Bapi_Limit << std::endl;
}
