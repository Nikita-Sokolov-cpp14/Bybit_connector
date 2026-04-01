#include "position.h"
#include <iostream>

void PositionHFT::print() {
    std::cout << "id " << id << std::endl;
    std::cout << "topic " << topic << std::endl;
    std::cout << "creationTime " << creationTime << std::endl;
    std::cout << "symbol " << symbol << std::endl;
    std::cout << "side " << side << std::endl;
    std::cout << "size " << size << std::endl;
    std::cout << "entryPrice " << entryPrice << std::endl;
    std::cout << "markPrice " << markPrice << std::endl;
    std::cout << "unrealisedPnl " << unrealisedPnl << std::endl;
    std::cout << "liqPrice " << liqPrice << std::endl;
    std::cout << "positionStatus " << positionStatus << std::endl;
    std::cout << "adlRankIndicator " << adlRankIndicator << std::endl;
    std::cout << "seq " << seq << std::endl;
    std::cout << "isReduceOnly " << isReduceOnly << std::endl;
}
