#pragma once

#include "data_structures/orderbook.h"
#include "data_structures/public_trade.h"

class ConnectionManager;

class BaseTradingStrategy {
public:
    virtual void setOrderbook(const OrderBook &orderbook) = 0;
    virtual void setPublicTrade(const PublicTrade &PublicTrade) = 0;

    void setConnectionManager(ConnectionManager *connectionManager);

private:
    ConnectionManager* connectionManager_;
};
