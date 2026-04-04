#include "order.h"
#include <iostream>

void OrderHFT::print() {
    std::cout << "topic " << topic << std::endl;
    std::cout << "id " << id << std::endl;
    std::cout << "creationTime " << creationTime << std::endl;
    std::cout << "symbol " << symbol << std::endl;
    std::cout << "orderId " << orderId << std::endl;
    std::cout << "orderLinkId " << orderLinkId << std::endl;
    std::cout << "side " << side << std::endl;
    std::cout << "orderStatus " << orderStatus << std::endl;
    std::cout << "timeInForce " << timeInForce << std::endl;
    std::cout << "price " << price << std::endl;
    std::cout << "qty " << qty << std::endl;
    std::cout << "avgPrice " << avgPrice << std::endl;
    std::cout << "leavesQty " << leavesQty << std::endl;
    std::cout << "cumExecQty " << cumExecQty << std::endl;
    std::cout << "orderType " << orderType << std::endl;
    std::cout << "reduceOnly " << reduceOnly << std::endl;
    std::cout << "createdTime " << createdTime << std::endl;
    std::cout << "updatedTime " << updatedTime << std::endl;
}
