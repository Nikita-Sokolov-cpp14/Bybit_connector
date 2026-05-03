#pragma once

#include "base_trading_strategy.h"

class ImbalanceAndLarde : public BaseTradingStrategy {
public:
    virtual void setOrderbook(const OrderBook &orderbook) override;
    virtual void setPublicTrade(const PublicTrade &PublicTrade) override;
};
