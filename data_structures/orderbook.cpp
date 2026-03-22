#include "orderbook.h"
#include <iostream>

OrderBook::OrderBook() :
topic(""),
ts(0),
s(""),
bids(),
asks(),
u(0),
seq(0),
cts(0) {
//     bids.reserve(1500);
//     asks.reserve(1500);
}

void OrderBook::clearLevels() {
    bids.clear();
    asks.clear();
}

void OrderBook::print() {
    std::cout << "topic: " << topic << std::endl;
    std::cout << "ts: " << ts << std::endl;
    std::cout << " === data == " << std::endl;
    std::cout << "s: " << s << std::endl;
    std::cout << " asks " << std::endl;
    for (const auto &val : asks) {
        std::cout << val.first << " " << val.second << std::endl;
    }
    std::cout << " bids " << std::endl;
    for (const auto &val : bids) {
        std::cout << val.first << " " << val.second << std::endl;
    }
    std::cout << "u: " << u << std::endl;
    std::cout << "seq: " << seq << std::endl;
    std::cout << " === end data == " << std::endl;
    std::cout << "cts: " << cts << std::endl;
}
