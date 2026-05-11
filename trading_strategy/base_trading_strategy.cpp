#include "base_trading_strategy.h"

BaseTradingStrategy::~BaseTradingStrategy() {
    stop();
}

void BaseTradingStrategy::start() {
}

void BaseTradingStrategy::stop() {
}

bool BaseTradingStrategy::sendOrder(const OrderRequest &request) {
    return false;
}

void BaseTradingStrategy::work() {
}
