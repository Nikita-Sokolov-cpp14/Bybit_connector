#include "execution_fast.h"
#include <iostream>

void ExecutionFast::print() {
    std::cout << "topic " << topic << std::endl;
    std::cout << "creationTime " << creationTime << std::endl;
    std::cout << "category " << category << std::endl;
    std::cout << "symbol " << symbol << std::endl;
    std::cout << "execId " << execId << std::endl;
    std::cout << "execPrice " << execPrice << std::endl;
    std::cout << "execQty " << execQty << std::endl;
    std::cout << "orderId " << orderId << std::endl;
    std::cout << "isMaker " << isMaker << std::endl;
    std::cout << "orderLinkId " << orderLinkId << std::endl;
    std::cout << "side " << side << std::endl;
    std::cout << "execTime " << execTime << std::endl;
    std::cout << "seq " << seq << std::endl;
}
