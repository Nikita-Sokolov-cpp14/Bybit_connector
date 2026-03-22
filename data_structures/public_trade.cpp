#include <iostream>

#include "public_trade.h"

void PublicTrade::clearData() {
    data.clear();
}

void PublicTrade::print() {
    std::cout << "=== publicTrade === " << std::endl;
    std::cout << "pair " << pairStr << std::endl;
    std::cout << "ts " << ts << std::endl;
    std::cout << "data size: " << data.size() << std::endl;
    for (const auto& trade : data) {
        std::cout << "trade data" << std::endl;
        std::cout << "T " << trade.T << std::endl;
        std::cout << "s " << trade.s << std::endl;
        std::cout << "S " << trade.S << std::endl;
        std::cout << "v " << trade.v << std::endl;
        std::cout << "p " << trade.p << std::endl;
        std::cout << "L " << trade.L << std::endl;
        std::cout << "i " << trade.i << std::endl;
        std::cout << "BT " << trade.BT << std::endl;
        std::cout << "RPI " << trade.RPI << std::endl;
        std::cout << "seq " << trade.seq << std::endl;
    }
}
