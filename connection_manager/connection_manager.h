#pragma once

#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>

#include "websocket_client/base_websocket_client.h"
#include "websocket_client/private_data_handler.h"
#include "websocket_client/order_sender.h"
#include "websocket_client/public_data_handler.h"
#include "json_parser/orderbook_json_parser.h"
#include "json_parser/public_trade_json_parser.h"
#include "utils/config.h"
#include "utils/ini_reader.h"
#include "trading_strategy/base_trading_strategy.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    void connect();

    void reconnectOrderSender();
    void reconnectPublicHandler();
    void reconnectPrivateHandler();

    bool placeOrder(const OrderRequest &orderRequest);

    void subscribeStrategy(BaseTradingStrategy *tradingStrategy);

    // Запретить копирование
    ConnectionManager(const ConnectionManager &) = delete;
    ConnectionManager &operator=(const ConnectionManager &) = delete;
    ConnectionManager(ConnectionManager &&) = delete;
    ConnectionManager &operator=(ConnectionManager &&) = delete;

private:
    AuthConfig authConfig_;
    ConnectionConfig connectionConfig_;
    net::io_context publicIoc_;
    net::io_context privateIoc_;
    net::io_context orderSenderIoc_;
    ssl::context sslCtx_;

    OrderBook orderBook_;
    StatusMessage statusMessage_;
    PublicTrade publicTrade_;

    PositionHFT positionHFT_;
    ExecutionFast executionFast_;
    OrderHFT orderHFT_;
    WalletHFT walletHFT_;
    PrivateDataHandler::Messages messages_;

    std::shared_ptr<PublicDataHandler> publicDataHandler_;
    std::shared_ptr<PrivateDataHandler> privateDataHandler_;
    std::shared_ptr<OrderSender> orderSender_;

    std::unique_ptr<std::jthread> publicThread_;
    std::unique_ptr<std::jthread> privateThread_;
    std::unique_ptr<std::jthread> orderSenderThread_;
    BaseTradingStrategy *tradingStrategy_;

    ssl::context createSSLContext();

    void setPublicReconnectCallback();

    void setPrivateReconnectCallback();

    void setOrderSenderReconnectCallback();

    void notifyOrderbookUpdate();
    void notifyTradeUpdate();
    void notifyOrderUpdate();
    void notifyPositionUpdate();

    // Настройка колбэков для PublicDataHandler
    void setupPublicDataCallbacks();

    void setupPrivateDataCallbacks();

    void setStrategyOrderSender();
};
